/*
 * Copyright (c) 2021 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_test, CONFIG_NET_SOCKETS_LOG_LEVEL);

#include <zephyr/ztest_assert.h>
#include <fcntl.h>
#include <zephyr/net/loopback.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include "../../socket_helpers.h"

#define TEST_STR_SMALL "test"

#define MY_IPV4_ADDR "127.0.0.1"
#define MY_IPV6_ADDR "::1"

#define ANY_PORT 0
#define SERVER_PORT 4242

#define PSK_TAG 1

#define MAX_CONNS 5

#define TCP_TEARDOWN_TIMEOUT K_MSEC(CONFIG_NET_TCP_TIME_WAIT_DELAY)

#define TLS_TEST_WORK_QUEUE_STACK_SIZE 3072

K_THREAD_STACK_DEFINE(tls_test_work_queue_stack, TLS_TEST_WORK_QUEUE_STACK_SIZE);
static struct k_work_q tls_test_work_queue;

static void test_work_reschedule(struct k_work_delayable *dwork,
				 k_timeout_t delay)
{
	k_work_reschedule_for_queue(&tls_test_work_queue, dwork, delay);
}

static void test_work_wait(struct k_work_delayable *dwork)
{
	struct k_work_sync sync;

	k_work_cancel_delayable_sync(dwork, &sync);
}

static const unsigned char psk[] = {
	0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};
static const char psk_id[] = "test_identity";

static void test_config_psk(int s_sock, int c_sock)
{
	sec_tag_t sec_tag_list[] = {
		PSK_TAG
	};

	(void)tls_credential_delete(PSK_TAG, TLS_CREDENTIAL_PSK);
	(void)tls_credential_delete(PSK_TAG, TLS_CREDENTIAL_PSK_ID);

	zassert_equal(tls_credential_add(PSK_TAG, TLS_CREDENTIAL_PSK,
					 psk, sizeof(psk)),
		      0, "Failed to register PSK %d");
	zassert_equal(tls_credential_add(PSK_TAG, TLS_CREDENTIAL_PSK_ID,
					 psk_id, strlen(psk_id)),
		      0, "Failed to register PSK ID");

	if (s_sock >= 0) {
		zassert_equal(setsockopt(s_sock, SOL_TLS, TLS_SEC_TAG_LIST,
					 sec_tag_list, sizeof(sec_tag_list)),
			      0, "Failed to set PSK on server socket");
	}

	if (c_sock >= 0) {
		zassert_equal(setsockopt(c_sock, SOL_TLS, TLS_SEC_TAG_LIST,
					 sec_tag_list, sizeof(sec_tag_list)),
			      0, "Failed to set PSK on client socket");
	}
}

static void test_fcntl(int sock, int cmd, int val)
{
	zassert_equal(fcntl(sock, cmd, val), 0, "fcntl failed");
}

static void test_bind(int sock, struct sockaddr *addr, socklen_t addrlen)
{
	zassert_equal(bind(sock, addr, addrlen),
		      0,
		      "bind failed");
}

static void test_listen(int sock)
{
	zassert_equal(listen(sock, MAX_CONNS),
		      0,
		      "listen failed");
}

static void test_connect(int sock, struct sockaddr *addr, socklen_t addrlen)
{
	k_yield();

	zassert_equal(connect(sock, addr, addrlen),
		      0,
		      "connect failed");

	if (IS_ENABLED(CONFIG_NET_TC_THREAD_PREEMPTIVE)) {
		/* Let the connection proceed */
		k_yield();
	}
}

static void test_send(int sock, const void *buf, size_t len, int flags)
{
	zassert_equal(send(sock, buf, len, flags),
		      len,
		      "send failed");
}

static void test_sendmsg(int sock, const struct msghdr *msg, int flags)
{
	size_t total_len = 0;

	for (int i = 0; i < msg->msg_iovlen; i++) {
		struct iovec *vec = msg->msg_iov + i;

		total_len += vec->iov_len;
	}

	zassert_equal(sendmsg(sock, msg, flags),
		      total_len,
		      "sendmsg failed");
}

static void test_accept(int sock, int *new_sock, struct sockaddr *addr,
			socklen_t *addrlen)
{
	zassert_not_null(new_sock, "null newsock");

	*new_sock = accept(sock, addr, addrlen);
	zassert_true(*new_sock >= 0, "accept failed");
}

static void test_shutdown(int sock, int how)
{
	zassert_equal(shutdown(sock, how),
		      0,
		      "shutdown failed");
}

static void test_close(int sock)
{
	zassert_equal(close(sock),
		      0,
		      "close failed");
}

static void test_eof(int sock)
{
	char rx_buf[1];
	ssize_t recved;

	/* Test that EOF properly detected. */
	recved = recv(sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(recved, 0, "");

	/* Calling again should be OK. */
	recved = recv(sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(recved, 0, "");

	/* Calling when TCP connection is fully torn down should be still OK. */
	k_sleep(TCP_TEARDOWN_TIMEOUT);
	recved = recv(sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(recved, 0, "");
}

ZTEST(net_socket_tls, test_so_type)
{
	struct sockaddr_in bind_addr4;
	struct sockaddr_in6 bind_addr6;
	int sock1, sock2, rv;
	int optval;
	socklen_t optlen = sizeof(optval);

	prepare_sock_tls_v4(MY_IPV4_ADDR, ANY_PORT, &sock1, &bind_addr4, IPPROTO_TLS_1_2);
	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &sock2, &bind_addr6, IPPROTO_TLS_1_2);

	rv = getsockopt(sock1, SOL_SOCKET, SO_TYPE, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, SOCK_STREAM, "getsockopt got invalid type");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	rv = getsockopt(sock2, SOL_SOCKET, SO_TYPE, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, SOCK_STREAM, "getsockopt got invalid type");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	test_close(sock1);
	test_close(sock2);
}

ZTEST(net_socket_tls, test_so_protocol)
{
	struct sockaddr_in bind_addr4;
	struct sockaddr_in6 bind_addr6;
	int sock1, sock2, rv;
	int optval;
	socklen_t optlen = sizeof(optval);

	prepare_sock_tls_v4(MY_IPV4_ADDR, ANY_PORT, &sock1, &bind_addr4, IPPROTO_TLS_1_2);
	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &sock2, &bind_addr6, IPPROTO_TLS_1_1);

	rv = getsockopt(sock1, SOL_SOCKET, SO_PROTOCOL, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, IPPROTO_TLS_1_2,
		      "getsockopt got invalid protocol");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	rv = getsockopt(sock2, SOL_SOCKET, SO_PROTOCOL, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, IPPROTO_TLS_1_1,
		      "getsockopt got invalid protocol");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	test_close(sock1);
	test_close(sock2);
}

struct test_msg_waitall_data {
	struct k_work_delayable tx_work;
	int sock;
	const uint8_t *data;
	size_t offset;
	int retries;
};

static void test_msg_waitall_tx_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct test_msg_waitall_data *test_data =
		CONTAINER_OF(dwork, struct test_msg_waitall_data, tx_work);

	if (test_data->retries > 0) {
		test_send(test_data->sock, test_data->data + test_data->offset, 1, 0);
		test_data->offset++;
		test_data->retries--;
		test_work_reschedule(&test_data->tx_work, K_MSEC(10));
	}
}

struct connect_data {
	struct k_work_delayable work;
	int sock;
	struct sockaddr *addr;
};

static void client_connect_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct connect_data *data =
		CONTAINER_OF(dwork, struct connect_data, work);

	test_connect(data->sock, data->addr, data->addr->sa_family == AF_INET ?
		     sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
}

static void test_prepare_tls_connection(sa_family_t family, int *c_sock,
					int *s_sock, int *new_sock)
{
	struct sockaddr c_saddr;
	struct sockaddr s_saddr;
	socklen_t exp_addrlen = family == AF_INET6 ?
				sizeof(struct sockaddr_in6) :
				sizeof(struct sockaddr_in);
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	struct connect_data test_data;

	if (family == AF_INET6) {
		prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, c_sock,
				    (struct sockaddr_in6 *)&c_saddr,
				    IPPROTO_TLS_1_2);
		prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, s_sock,
				    (struct sockaddr_in6 *)&s_saddr,
				    IPPROTO_TLS_1_2);
	} else {
		prepare_sock_tls_v4(MY_IPV4_ADDR, ANY_PORT, c_sock,
				    (struct sockaddr_in *)&c_saddr,
				    IPPROTO_TLS_1_2);
		prepare_sock_tls_v4(MY_IPV4_ADDR, ANY_PORT, s_sock,
				    (struct sockaddr_in *)&s_saddr,
				    IPPROTO_TLS_1_2);
	}

	test_config_psk(*s_sock, *c_sock);

	test_bind(*s_sock, &s_saddr, exp_addrlen);
	test_listen(*s_sock);

	/* Helper work for the connect operation - need to handle client/server
	 * in parallel due to handshake.
	 */
	test_data.sock = *c_sock;
	test_data.addr = &s_saddr;
	k_work_init_delayable(&test_data.work, client_connect_work_handler);
	test_work_reschedule(&test_data.work, K_NO_WAIT);

	test_accept(*s_sock, new_sock, &addr, &addrlen);
	zassert_equal(addrlen, exp_addrlen, "Wrong addrlen");

	test_work_wait(&test_data.work);
}

ZTEST(net_socket_tls, test_v4_msg_waitall)
{
	struct test_msg_waitall_data test_data = {
		.data = TEST_STR_SMALL,
	};
	int c_sock;
	int s_sock;
	int new_sock;
	int ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	struct timeval timeo_optval = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};

	test_prepare_tls_connection(AF_INET, &c_sock, &s_sock, &new_sock);

	/* Regular MSG_WAITALL - make sure recv returns only after
	 * requested amount is received.
	 */
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf);
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf), MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf), "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf),
			  "Invalid data received");
	test_work_wait(&test_data.tx_work);

	/* MSG_WAITALL + SO_RCVTIMEO - make sure recv returns the amount of data
	 * received so far
	 */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, &timeo_optval,
			 sizeof(timeo_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	memset(rx_buf, 0, sizeof(rx_buf));
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf) - 1;
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf) - 1, MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf) - 1, "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf) - 1,
			  "Invalid data received");
	test_work_wait(&test_data.tx_work);

	test_close(new_sock);
	test_close(s_sock);
	test_close(c_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_v6_msg_waitall)
{
	struct test_msg_waitall_data test_data = {
		.data = TEST_STR_SMALL,
	};
	int c_sock;
	int s_sock;
	int new_sock;
	int ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	struct timeval timeo_optval = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Regular MSG_WAITALL - make sure recv returns only after
	 * requested amount is received.
	 */
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf);
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf), MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf), "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf),
			  "Invalid data received");
	test_work_wait(&test_data.tx_work);

	/* MSG_WAITALL + SO_RCVTIMEO - make sure recv returns the amount of data
	 * received so far
	 */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, &timeo_optval,
			 sizeof(timeo_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	memset(rx_buf, 0, sizeof(rx_buf));
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf) - 1;
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf) - 1, MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf) - 1, "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf) - 1,
			  "Invalid data received");
	test_work_wait(&test_data.tx_work);

	test_close(new_sock);
	test_close(s_sock);
	test_close(c_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

struct send_data {
	struct k_work_delayable tx_work;
	int sock;
	const uint8_t *data;
	size_t datalen;
};

static void send_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct send_data *test_data =
		CONTAINER_OF(dwork, struct send_data, tx_work);

	test_send(test_data->sock, test_data->data, test_data->datalen, 0);
}

void test_msg_trunc(int sock_c, int sock_s, struct sockaddr *addr_c,
		    socklen_t addrlen_c, struct sockaddr *addr_s,
		    socklen_t addrlen_s)
{
	int rv;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1];
	int role = TLS_DTLS_ROLE_SERVER;
	struct send_data test_data = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};

	test_config_psk(sock_s, sock_c);

	rv = setsockopt(sock_s, SOL_TLS, TLS_DTLS_ROLE, &role, sizeof(role));
	zassert_equal(rv, 0, "failed to set DTLS server role");

	rv = bind(sock_s, addr_s, addrlen_s);
	zassert_equal(rv, 0, "server bind failed");

	rv = bind(sock_c, addr_c, addrlen_c);
	zassert_equal(rv, 0, "client bind failed");

	rv = connect(sock_c, addr_s, addrlen_s);
	zassert_equal(rv, 0, "connect failed");

	/* MSG_TRUNC */

	test_data.sock = sock_c;
	k_work_init_delayable(&test_data.tx_work, send_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	memset(rx_buf, 0, sizeof(rx_buf));
	rv = recv(sock_s, rx_buf, 2, ZSOCK_MSG_TRUNC);
	zassert_equal(rv, sizeof(TEST_STR_SMALL) - 1, "MSG_TRUNC flag failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, 2, "invalid rx data");
	zassert_equal(rx_buf[2], 0, "received more than requested");

	/* The remaining data should've been discarded */
	rv = recv(sock_s, rx_buf, sizeof(rx_buf), ZSOCK_MSG_DONTWAIT);
	zassert_equal(rv, -1, "consecutive recv should've failed");
	zassert_equal(errno, EAGAIN, "incorrect errno value");

	/* MSG_PEEK not supported by DTLS socket */

	rv = close(sock_c);
	zassert_equal(rv, 0, "close failed");
	rv = close(sock_s);
	zassert_equal(rv, 0, "close failed");

	test_work_wait(&test_data.tx_work);
}

ZTEST(net_socket_tls, test_v4_msg_trunc)
{
	int client_sock;
	int server_sock;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;

	prepare_sock_dtls_v4(MY_IPV4_ADDR, ANY_PORT, &client_sock, &client_addr, IPPROTO_DTLS_1_2);
	prepare_sock_dtls_v4(MY_IPV4_ADDR, ANY_PORT, &server_sock, &server_addr, IPPROTO_DTLS_1_2);

	test_msg_trunc(client_sock, server_sock,
		       (struct sockaddr *)&client_addr, sizeof(client_addr),
		       (struct sockaddr *)&server_addr, sizeof(server_addr));
}

ZTEST(net_socket_tls, test_v6_msg_trunc)
{
	int client_sock;
	int server_sock;
	struct sockaddr_in6 client_addr;
	struct sockaddr_in6 server_addr;

	prepare_sock_dtls_v6(MY_IPV6_ADDR, ANY_PORT, &client_sock, &client_addr, IPPROTO_DTLS_1_2);
	prepare_sock_dtls_v6(MY_IPV6_ADDR, ANY_PORT, &server_sock, &server_addr, IPPROTO_DTLS_1_2);

	test_msg_trunc(client_sock, server_sock,
		       (struct sockaddr *)&client_addr, sizeof(client_addr),
		       (struct sockaddr *)&server_addr, sizeof(server_addr));
}

struct test_sendmsg_data {
	struct k_work_delayable tx_work;
	int sock;
	const struct msghdr *msg;
};

static void test_sendmsg_tx_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct test_sendmsg_data *test_data =
		CONTAINER_OF(dwork, struct test_sendmsg_data, tx_work);

	test_sendmsg(test_data->sock, test_data->msg, 0);
}

static void test_dtls_sendmsg(int sock_c, int sock_s, struct sockaddr *addr_c,
			      socklen_t addrlen_c, struct sockaddr *addr_s,
			      socklen_t addrlen_s)
{
	int rv;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1];
	int role = TLS_DTLS_ROLE_SERVER;
	struct iovec iov[3] = {
		{},
		{
			.iov_base = TEST_STR_SMALL,
			.iov_len = sizeof(TEST_STR_SMALL) - 1,
		},
		{},
	};
	struct msghdr msg = {};
	struct test_sendmsg_data test_data = {
		.msg = &msg,
	};

	test_config_psk(sock_s, sock_c);

	rv = setsockopt(sock_s, SOL_TLS, TLS_DTLS_ROLE, &role, sizeof(role));
	zassert_equal(rv, 0, "failed to set DTLS server role");

	rv = bind(sock_s, addr_s, addrlen_s);
	zassert_equal(rv, 0, "server bind failed");

	rv = bind(sock_c, addr_c, addrlen_c);
	zassert_equal(rv, 0, "client bind failed");

	rv = connect(sock_c, addr_s, addrlen_s);
	zassert_equal(rv, 0, "connect failed");

	test_data.sock = sock_c;
	k_work_init_delayable(&test_data.tx_work, test_sendmsg_tx_work_handler);

	/* sendmsg() with single fragment */

	msg.msg_iov = &iov[1];
	msg.msg_iovlen = 1,

	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	memset(rx_buf, 0, sizeof(rx_buf));
	rv = recv(sock_s, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(rv, sizeof(TEST_STR_SMALL) - 1, "recv failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(TEST_STR_SMALL) - 1, "invalid rx data");

	test_work_wait(&test_data.tx_work);

	/* sendmsg() with single non-empty fragment */

	msg.msg_iov = iov;
	msg.msg_iovlen = ARRAY_SIZE(iov);

	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	memset(rx_buf, 0, sizeof(rx_buf));
	rv = recv(sock_s, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(rv, sizeof(TEST_STR_SMALL) - 1, "recv failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(TEST_STR_SMALL) - 1, "invalid rx data");

	test_work_wait(&test_data.tx_work);

	/* sendmsg() with multiple non-empty fragments */

	iov[0].iov_base = TEST_STR_SMALL;
	iov[0].iov_len = sizeof(TEST_STR_SMALL) - 1;

	rv = sendmsg(sock_c, &msg, 0);
	zassert_equal(rv, -1, "sendmsg succeeded");
	zassert_equal(errno, EMSGSIZE, "incorrect errno value");

	rv = close(sock_c);
	zassert_equal(rv, 0, "close failed");
	rv = close(sock_s);
	zassert_equal(rv, 0, "close failed");
}

ZTEST(net_socket_tls, test_v4_dtls_sendmsg)
{
	int client_sock;
	int server_sock;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;

	prepare_sock_dtls_v4(MY_IPV4_ADDR, ANY_PORT, &client_sock, &client_addr, IPPROTO_DTLS_1_2);
	prepare_sock_dtls_v4(MY_IPV4_ADDR, ANY_PORT, &server_sock, &server_addr, IPPROTO_DTLS_1_2);

	test_dtls_sendmsg(client_sock, server_sock,
			  (struct sockaddr *)&client_addr, sizeof(client_addr),
			  (struct sockaddr *)&server_addr, sizeof(server_addr));
}

ZTEST(net_socket_tls, test_v6_dtls_sendmsg)
{
	int client_sock;
	int server_sock;
	struct sockaddr_in6 client_addr;
	struct sockaddr_in6 server_addr;

	prepare_sock_dtls_v6(MY_IPV6_ADDR, ANY_PORT, &client_sock, &client_addr, IPPROTO_DTLS_1_2);
	prepare_sock_dtls_v6(MY_IPV6_ADDR, ANY_PORT, &server_sock, &server_addr, IPPROTO_DTLS_1_2);

	test_dtls_sendmsg(client_sock, server_sock,
			  (struct sockaddr *)&client_addr, sizeof(client_addr),
			  (struct sockaddr *)&server_addr, sizeof(server_addr));
}

struct close_data {
	struct k_work_delayable work;
	int fd;
};

static void close_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct close_data *data = CONTAINER_OF(dwork, struct close_data, work);

	close(data->fd);
}

ZTEST(net_socket_tls, test_close_while_accept)
{
	int s_sock;
	int new_sock;
	struct sockaddr_in6 s_saddr;
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	struct close_data close_work_data;

	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &s_sock, &s_saddr, IPPROTO_TLS_1_2);

	test_config_psk(s_sock, -1);

	test_bind(s_sock, (struct sockaddr *)&s_saddr, sizeof(s_saddr));
	test_listen(s_sock);

	/* Schedule close() from workqueue */
	k_work_init_delayable(&close_work_data.work, close_work);
	close_work_data.fd = s_sock;
	test_work_reschedule(&close_work_data.work, K_MSEC(10));

	/* Start blocking accept(), which should be unblocked by close() from
	 * another thread and return an error.
	 */
	new_sock = accept(s_sock, &addr, &addrlen);
	zassert_equal(new_sock, -1, "accept did not return error");
	zassert_equal(errno, EINTR, "Unexpected errno value: %d", errno);

	test_work_wait(&close_work_data.work);
}

ZTEST(net_socket_tls, test_close_while_recv)
{
	int c_sock, s_sock, new_sock, ret;
	struct close_data close_work_data;
	uint8_t rx_buf;

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Schedule close() from workqueue */
	k_work_init_delayable(&close_work_data.work, close_work);
	close_work_data.fd = new_sock;
	test_work_reschedule(&close_work_data.work, K_MSEC(10));

	ret = recv(new_sock, &rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, -1, "recv did not return error");
	zassert_equal(errno, EINTR, "Unexpected errno value: %d", errno);

	test_close(s_sock);
	test_close(c_sock);

	test_work_wait(&close_work_data.work);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_connect_timeout)
{
	struct sockaddr_in6 c_saddr;
	struct sockaddr_in6 s_saddr;
	int c_sock;
	int ret;

	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &c_sock, &c_saddr,
			    IPPROTO_TLS_1_2);
	test_config_psk(-1, c_sock);

	s_saddr.sin6_family = AF_INET6;
	s_saddr.sin6_port = htons(SERVER_PORT);
	ret = zsock_inet_pton(AF_INET6, MY_IPV6_ADDR, &s_saddr.sin6_addr);
	zassert_equal(ret, 1, "inet_pton failed");

	loopback_set_packet_drop_ratio(1.0f);

	zassert_equal(connect(c_sock, (struct sockaddr *)&s_saddr,
			      sizeof(s_saddr)),
		      -1, "connect succeed");
	zassert_equal(errno, ETIMEDOUT,
		      "connect should be timed out, got %d", errno);

	test_close(c_sock);

	loopback_set_packet_drop_ratio(0.0f);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_connect_closed_port)
{
	struct sockaddr_in6 c_saddr;
	struct sockaddr_in6 s_saddr;
	int c_sock;
	int ret;

	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &c_sock, &c_saddr,
			    IPPROTO_TLS_1_2);
	test_config_psk(-1, c_sock);

	s_saddr.sin6_family = AF_INET6;
	s_saddr.sin6_port = htons(SERVER_PORT);
	ret = zsock_inet_pton(AF_INET6, MY_IPV6_ADDR, &s_saddr.sin6_addr);
	zassert_equal(ret, 1, "inet_pton failed");

	zassert_equal(connect(c_sock, (struct sockaddr *)&s_saddr,
			      sizeof(s_saddr)),
		      -1, "connect succeed");
	zassert_equal(errno, ETIMEDOUT,
		      "connect should fail, got %d", errno);

	test_close(c_sock);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

struct fake_tcp_server_data {
	struct k_work_delayable work;
	int sock;
	bool reply;
};

static void fake_tcp_server_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct fake_tcp_server_data *data =
		CONTAINER_OF(dwork, struct fake_tcp_server_data, work);
	int new_sock;

	test_accept(data->sock, &new_sock, NULL, 0);

	if (!data->reply) {
		goto out;
	}

	while (true) {
		int ret;
		char rx_buf[32];

		ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
		if (ret <= 0) {
			break;
		}

		(void)send(new_sock, TEST_STR_SMALL, sizeof(TEST_STR_SMALL), 0);
	}

out:
	test_close(new_sock);
}

static void test_prepare_fake_tcp_server(struct fake_tcp_server_data *s_data,
					 sa_family_t family, int *s_sock,
					 struct sockaddr *s_saddr, bool reply)
{
	socklen_t exp_addrlen = family == AF_INET6 ?
				sizeof(struct sockaddr_in6) :
				sizeof(struct sockaddr_in);

	if (family == AF_INET6) {
		prepare_sock_tcp_v6(MY_IPV6_ADDR, SERVER_PORT, s_sock,
				    (struct sockaddr_in6 *)s_saddr);
	} else {
		prepare_sock_tcp_v4(MY_IPV4_ADDR, SERVER_PORT, s_sock,
				    (struct sockaddr_in *)s_saddr);
	}

	test_bind(*s_sock, s_saddr, exp_addrlen);
	test_listen(*s_sock);

	s_data->sock = *s_sock;
	s_data->reply = reply;
	k_work_init_delayable(&s_data->work, fake_tcp_server_work);
	test_work_reschedule(&s_data->work, K_NO_WAIT);
}

ZTEST(net_socket_tls, test_connect_invalid_handshake_data)
{
	struct fake_tcp_server_data server_data;
	struct sockaddr_in6 c_saddr;
	struct sockaddr_in6 s_saddr;
	int c_sock, s_sock;

	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &c_sock, &c_saddr,
			    IPPROTO_TLS_1_2);
	test_config_psk(-1, c_sock);
	test_prepare_fake_tcp_server(&server_data, AF_INET6, &s_sock,
				     (struct sockaddr *)&s_saddr, true);

	zassert_equal(connect(c_sock, (struct sockaddr *)&s_saddr,
			      sizeof(s_saddr)),
		      -1, "connect succeed");
	zassert_equal(errno, ECONNABORTED,
		      "connect should fail, got %d", errno);

	test_close(c_sock);
	test_close(s_sock);

	test_work_wait(&server_data.work);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_connect_no_handshake_data)
{
	struct fake_tcp_server_data server_data;
	struct sockaddr_in6 c_saddr;
	struct sockaddr s_saddr;
	int c_sock, s_sock;

	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &c_sock, &c_saddr,
			    IPPROTO_TLS_1_2);
	test_config_psk(-1, c_sock);
	test_prepare_fake_tcp_server(&server_data, AF_INET6, &s_sock,
				     (struct sockaddr *)&s_saddr, false);

	zassert_equal(connect(c_sock, (struct sockaddr *)&s_saddr,
			      sizeof(s_saddr)),
		      -1, "connect succeed");
	zassert_equal(errno, ECONNABORTED,
		      "connect should fail, got %d", errno);

	test_close(c_sock);
	test_close(s_sock);

	test_work_wait(&server_data.work);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_accept_non_block)
{
	int s_sock, new_sock;
	uint32_t timestamp;
	struct sockaddr_in6 s_saddr;

	prepare_sock_tls_v6(MY_IPV6_ADDR, SERVER_PORT, &s_sock, &s_saddr,
			    IPPROTO_TLS_1_2);

	test_config_psk(s_sock, -1);
	test_fcntl(s_sock, F_SETFL, O_NONBLOCK);
	test_bind(s_sock, (struct sockaddr *)&s_saddr, sizeof(s_saddr));
	test_listen(s_sock);

	timestamp = k_uptime_get_32();
	new_sock = accept(s_sock, NULL, NULL);
	zassert_true(k_uptime_get_32() - timestamp <= 100, "");
	zassert_equal(new_sock, -1, "accept did not return error");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	test_close(s_sock);
}

ZTEST(net_socket_tls, test_accept_invalid_handshake_data)
{
	int s_sock, c_sock, new_sock;
	struct sockaddr_in6 s_saddr;
	struct sockaddr_in6 c_saddr;

	prepare_sock_tls_v6(MY_IPV6_ADDR, ANY_PORT, &s_sock, &s_saddr,
			    IPPROTO_TLS_1_2);
	prepare_sock_tcp_v6(MY_IPV6_ADDR, ANY_PORT, &c_sock, &c_saddr);

	test_config_psk(s_sock, -1);
	test_bind(s_sock, (struct sockaddr *)&s_saddr, sizeof(s_saddr));
	test_listen(s_sock);

	/* Connect at TCP level and send some unexpected data. */
	test_connect(c_sock, (struct sockaddr *)&s_saddr, sizeof(s_saddr));
	test_send(c_sock, TEST_STR_SMALL, sizeof(TEST_STR_SMALL), 0);

	new_sock = accept(s_sock, NULL, NULL);
	zassert_equal(new_sock, -1, "accept did not return error");
	zassert_equal(errno, ECONNABORTED, "Unexpected errno value: %d", errno);

	test_close(s_sock);
	test_close(c_sock);
}

ZTEST(net_socket_tls, test_recv_non_block)
{
	int c_sock, s_sock, new_sock, ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Verify ZSOCK_MSG_DONTWAIT flag first */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), ZSOCK_MSG_DONTWAIT);
	zassert_equal(ret, -1, "recv()) should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	/* Verify fcntl and O_NONBLOCK */
	test_fcntl(new_sock, F_SETFL, O_NONBLOCK);
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, -1, "recv() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "send() failed");

	/* Let the data got through. */
	k_sleep(K_MSEC(10));

	/* Should get data now */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	/* And EAGAIN on consecutive read */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, -1, "recv() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_recv_block)
{
	int c_sock, s_sock, new_sock, ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	struct send_data test_data = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work, send_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	/* recv() shall block until send work sends the data. */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, sizeof(TEST_STR_SMALL) - 1, "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

#define TLS_RECORD_OVERHEAD 81

ZTEST(net_socket_tls, test_send_non_block)
{
	int c_sock, s_sock, new_sock, ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	int buf_optval = TLS_RECORD_OVERHEAD + sizeof(TEST_STR_SMALL) - 1;

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Simulate window full scenario with SO_RCVBUF option. */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVBUF, &buf_optval,
			 sizeof(buf_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	/* Fill out the window */
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "send() failed");

	/* Wait for ACK (empty window, min. 100 ms due to silly window
	 * protection).
	 */
	k_sleep(K_MSEC(150));

	/* Verify ZSOCK_MSG_DONTWAIT flag first */
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL),
		   ZSOCK_MSG_DONTWAIT);
	zassert_equal(ret, -1, "send() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	/* Verify fcntl and O_NONBLOCK */
	test_fcntl(c_sock, F_SETFL, O_NONBLOCK);
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, -1, "send() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	/* Wait for the window to update. */
	k_sleep(K_MSEC(10));

	/* Should succeed now. */
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "send() failed");

	/* Flush the data */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	/* And make sure there's no more data left. */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), ZSOCK_MSG_DONTWAIT);
	zassert_equal(ret, -1, "recv() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

struct recv_data {
	struct k_work_delayable work;
	int sock;
	const uint8_t *data;
	size_t datalen;
};

static void recv_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct recv_data *test_data = CONTAINER_OF(dwork, struct recv_data, work);
	char rx_buf[30] = { 0 };
	size_t off = 0;
	int ret;

	while (off < test_data->datalen) {
		size_t recvlen = MIN(sizeof(rx_buf), test_data->datalen - off);

		ret = recv(test_data->sock, rx_buf, recvlen, 0);
		zassert_true(ret > 0, "recv() error");
		zassert_mem_equal(rx_buf, test_data->data + off, ret,
				  "unexpected data");

		off += ret;
		zassert_true(off <= test_data->datalen,
			     "received more than expected");
	}
}

ZTEST(net_socket_tls, test_send_block)
{
	int c_sock, s_sock, new_sock, ret;
	int buf_optval = TLS_RECORD_OVERHEAD + sizeof(TEST_STR_SMALL) - 1;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	struct recv_data test_data = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Simulate window full scenario with SO_RCVBUF option. */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVBUF, &buf_optval,
			 sizeof(buf_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	/* Fill out the window */
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "send() failed");

	/* Wait for ACK (empty window, min. 100 ms due to silly window
	 * protection).
	 */
	k_sleep(K_MSEC(150));

	test_data.sock = new_sock;
	k_work_init_delayable(&test_data.work, recv_work_handler);
	test_work_reschedule(&test_data.work, K_MSEC(10));

	/* Should block and succeed. */
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "send() failed");

	/* Flush the data */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	/* And make sure there's no more data left. */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), ZSOCK_MSG_DONTWAIT);
	zassert_equal(ret, -1, "recv() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_so_rcvtimeo)
{
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1];
	uint32_t start_time, time_diff;
	struct timeval optval = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};
	struct send_data test_data = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};
	int c_sock, s_sock, new_sock, ret;

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	ret = setsockopt(c_sock, SOL_SOCKET, SO_RCVTIMEO, &optval,
			 sizeof(optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	start_time = k_uptime_get_32();
	ret = recv(c_sock, rx_buf, sizeof(rx_buf), 0);
	time_diff = k_uptime_get_32() - start_time;

	zassert_equal(ret, -1, "recv() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);
	zassert_true(time_diff >= 500, "Expected timeout after 500ms but "
		     "was %dms", time_diff);

	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work, send_work_handler);
	test_work_reschedule(&test_data.tx_work, K_MSEC(10));

	/* recv() shall return as soon as it gets data, regardless of timeout. */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, sizeof(TEST_STR_SMALL) - 1, "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_so_sndtimeo)
{
	int buf_optval = TLS_RECORD_OVERHEAD + sizeof(TEST_STR_SMALL) - 1;
	uint32_t start_time, time_diff;
	struct timeval timeo_optval = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};
	struct recv_data test_data = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};
	int c_sock, s_sock, new_sock, ret;

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	ret = setsockopt(c_sock, SOL_SOCKET, SO_SNDTIMEO, &timeo_optval,
			 sizeof(timeo_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	/* Simulate window full scenario with SO_RCVBUF option. */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVBUF, &buf_optval,
			 sizeof(buf_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	ret = send(c_sock, TEST_STR_SMALL, sizeof(TEST_STR_SMALL) - 1, 0);
	zassert_equal(ret, sizeof(TEST_STR_SMALL) - 1, "send() failed");

	/* Wait for ACK (empty window). */
	k_msleep(150);

	/* Client should not be able to send now and time out after SO_SNDTIMEO */
	start_time = k_uptime_get_32();
	ret = send(c_sock, TEST_STR_SMALL, sizeof(TEST_STR_SMALL) - 1, 0);
	time_diff = k_uptime_get_32() - start_time;

	zassert_equal(ret, -1, "send() should've failed");
	zassert_equal(errno, EAGAIN, "Unexpected errno value: %d", errno);
	zassert_true(time_diff >= 500, "Expected timeout after 500ms but "
			"was %dms", time_diff);

	test_data.sock = new_sock;
	k_work_init_delayable(&test_data.work, recv_work_handler);
	test_work_reschedule(&test_data.work, K_MSEC(10));

	/* Should block and succeed. */
	ret = send(c_sock, TEST_STR_SMALL, strlen(TEST_STR_SMALL), 0);
	zassert_equal(ret, strlen(TEST_STR_SMALL), "send() failed");

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_shutdown_rd_synchronous)
{
	int c_sock, s_sock, new_sock;

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Shutdown reception */
	test_shutdown(c_sock, ZSOCK_SHUT_RD);

	/* EOF should be notified by recv() */
	test_eof(c_sock);

	test_close(new_sock);
	test_close(s_sock);
	test_close(c_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

struct shutdown_data {
	struct k_work_delayable work;
	int sock;
	int how;
};

static void shutdown_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct shutdown_data *data = CONTAINER_OF(dwork, struct shutdown_data,
						  work);

	shutdown(data->sock, data->how);
}

ZTEST(net_socket_tls, test_shutdown_rd_while_recv)
{
	int c_sock, s_sock, new_sock;
	struct shutdown_data test_data = {
		.how = ZSOCK_SHUT_RD,
	};

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	/* Schedule reception shutdown from workqueue */
	k_work_init_delayable(&test_data.work, shutdown_work);
	test_data.sock = c_sock;
	test_work_reschedule(&test_data.work, K_MSEC(10));

	/* EOF should be notified by recv() */
	test_eof(c_sock);

	test_close(new_sock);
	test_close(s_sock);
	test_close(c_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

ZTEST(net_socket_tls, test_send_while_recv)
{
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1];
	struct send_data test_data_c = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};
	struct send_data test_data_s = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};
	int c_sock, s_sock, new_sock, ret;

	test_prepare_tls_connection(AF_INET6, &c_sock, &s_sock, &new_sock);

	test_data_c.sock = c_sock;
	k_work_init_delayable(&test_data_c.tx_work, send_work_handler);
	test_work_reschedule(&test_data_c.tx_work, K_MSEC(10));

	test_data_s.sock = new_sock;
	k_work_init_delayable(&test_data_s.tx_work, send_work_handler);
	test_work_reschedule(&test_data_s.tx_work, K_MSEC(20));

	/* recv() shall block until the second work is executed. The second work
	 * will execute only if the first one won't block.
	 */
	ret = recv(c_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, sizeof(TEST_STR_SMALL) - 1, "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	/* Check if the server sock got its data. */
	ret = recv(new_sock, rx_buf, sizeof(rx_buf), 0);
	zassert_equal(ret, sizeof(TEST_STR_SMALL) - 1, "recv() failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, ret, "Invalid data received");

	test_close(c_sock);
	test_close(new_sock);
	test_close(s_sock);

	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

static void *tls_tests_setup(void)
{
	k_work_queue_init(&tls_test_work_queue);
	k_work_queue_start(&tls_test_work_queue, tls_test_work_queue_stack,
			   K_THREAD_STACK_SIZEOF(tls_test_work_queue_stack),
			   K_LOWEST_APPLICATION_THREAD_PRIO, NULL);

	return NULL;
}

ZTEST_SUITE(net_socket_tls, NULL, tls_tests_setup, NULL, NULL, NULL);
