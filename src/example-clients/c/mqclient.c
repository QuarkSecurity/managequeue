/**
 * Copyright (c) 2017 Quark Security, Inc.
 * Author: Marshall Miller <marshall@quarksecurity.com>
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <libconfig.h>

#define MSG_MAX 8192

const short int PROJ_ID = 15;

struct msg_queue_config {
	char* path;            /* the path used by ftok to locate a message queue */
	int msgqueue_id;       /* the located message queue id */
};

struct msgbuf {
	long mtype;            /* message type, must be > 0 */
	char mtext[MSG_MAX];   /* message data */
};

/* Reads options from a config file
 *
 * @param config          pointer to the config file path.
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int load_config(char *config, struct msg_queue_config* msgq_conf)
{
	int ret = -1;
	config_t cfg;
	const char *path = NULL;

	config_init(&cfg);

	if (config_read_file(&cfg, config) != CONFIG_TRUE) {
		fprintf(stderr, "Failed to read config file.\n");
		printf("%s\n", config_error_text(&cfg));
		goto done;
	}

	if (config_lookup_string(&cfg, "path", &path) != CONFIG_TRUE) {
		fprintf(stderr, "Failed to find \"path\" option: %s\n", config_error_text(&cfg));
		goto done;
	}

	msgq_conf->path = strdup(path);
	if (msgq_conf->path == NULL) {
		fprintf(stderr, "Error duplicating message queue path: %s\n", strerror(errno));
		goto done;
	}

	ret = 0;

done:
	config_destroy(&cfg);
	return ret;
}

/* Loads the parameters into the msg_queue_config struct.
 *
 * @param num_args          number of arguements being passsed
 * @param args              array of arguments
 * @param [out] msgq_conf   pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int load_parameters(int num_args, char **args, struct msg_queue_config* msgq_conf)
{
	int ret = -1;
	char *path = strdup(args[2]);
	if (path == NULL) {
		fprintf(stderr, "Error duplicating message queue path\n");
		goto done;
	}

	msgq_conf->path = path;

	ret = 0;

done:
	return ret;
}

/* Sends a message over a message queue
 *
 * @param msg             pointer to the message buffer to send
 * @param length          the size of the buffer to send
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int msg_queue_send(char *msg, int length, struct msg_queue_config* msgq_conf)
{
	int ret = -1;
	struct msgbuf mbuf;

	mbuf.mtype = 1;
	memcpy(mbuf.mtext, msg, length);

	if (msgsnd(msgq_conf->msgqueue_id, &mbuf, length, 0) == -1) {
		fprintf(stderr, "Failed to send message: %s\n", strerror(errno));
		goto done;
	}

	ret = 0;

done:
	return ret;
}

/* Receives a message from a message queue
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @param [out] msg       pointer to the message buffer
 * @param [in] length     the maximum size of the buffer
 * @param [out] received  the size received message
 * @return 0 for success -1 for failure
 */
int msg_queue_receive(char *msg, int length, int *received, struct msg_queue_config* msgq_conf)
{
	int ret = -1;
	struct msgbuf mbuf;
	int read_len;

	if ((read_len = msgrcv(msgq_conf->msgqueue_id, &mbuf, length, 0, 0)) == -1) {
		fprintf(stderr, "Failed to receive message: %s\n", strerror(errno));
		goto done;
	}

	*received = read_len;
	memcpy(msg, mbuf.mtext, read_len);
	ret = 0;

done:
	return ret;
}

/* Cleans up msg queue config struct
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
void config_cleanup(struct msg_queue_config* msgq_conf)
{
	if (msgq_conf->path) {
		free(msgq_conf->path);
	}
}

/* Determine the message queue id based on the given path and PROJ_ID
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int get_msg_queue(struct msg_queue_config *msgq_conf)
{
	int ret = -1;
	key_t key;
	int msgqueue_id;

	key = ftok(msgq_conf->path, PROJ_ID);
	if (key == -1) {
		fprintf(stderr,"Failed to get message queue key: %s\n", strerror(errno));
		goto done;
	}

	msgqueue_id = msgget(key, 0);
	if (msgqueue_id < 0) {
		fprintf(stderr, "Failed to get message queue ID: %s\n", strerror(errno));
		goto done;
	}
	msgq_conf->msgqueue_id = msgqueue_id;

	ret = 0;

done:
	return ret;
}

/* Read data from an input stream until the buffer is full or
 * the stream is closed or experiences an error
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return number of bytes read for success or -1 for failure
 */
int read_stream(char *buf, int buf_len, FILE *stream)
{
	int ret = -1;
	int bytes_read = 0;

	while (true) {
		bytes_read += fread(buf + bytes_read, 1, buf_len - bytes_read, stream);
		if (bytes_read > 0) {
			ret = bytes_read;
		}

		if (feof(stream)) {
			break;
		} else if (bytes_read == buf_len) {
			fprintf(stderr, "Input truncated to %d bytes\n", bytes_read);
			break;
		} else if (ferror(stream)) {
			fprintf(stderr, "Error reading from stream\n");
			break;
		}
	}
	return ret;
}

int main(int argc, char **argv)
{
	int ret = -1, opt;
	char *cfg_path = NULL;
	bool is_config = false;
	struct msg_queue_config msgq_conf = {0};
	char *cmd = argv[1];
	char buf[MSG_MAX];
	int buf_len = MSG_MAX;

	while ((opt = getopt(argc, argv, "c:")) != -1) {
		switch (opt) {
			case 'c':
				cfg_path = strdup(optarg);
				is_config = true;
				break;
			default:
				break;
		}
	}

	if (is_config) {
		if (load_config(cfg_path, &msgq_conf)) {
			goto done;
		}
	} else if (argc >= 3) {
		if (load_parameters(argc, argv, &msgq_conf)) {
			goto done;
		}
	} else {
		fprintf(stderr, "Invalid parameters\n");
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\tWithout Config:          %s <send/receive> <path>\n", argv[0]);
		fprintf(stderr, "\tWith Config:             %s <send/receive> -c <config file>\n", argv[0]);

		goto done;
	}

	if (get_msg_queue(&msgq_conf) != 0) {
		goto done;
	}

	if (strcasecmp(cmd,"send") == 0) {
		int read_len = read_stream(buf, buf_len, stdin);
		if (read_len < 0) {
			goto done;
		}
		if (msg_queue_send(buf, read_len, &msgq_conf) != 0) {
			goto done;
		}
	} else if (strcasecmp(cmd,"receive") == 0) {
		int receive_len;
		int write_len;
		if (msg_queue_receive(buf, buf_len, &receive_len, &msgq_conf) != 0) {
			goto done;
		}
		write_len = fwrite(buf, 1, receive_len, stdout);
		if (write_len != receive_len) {
			fprintf(stderr, "Failed to output the full message\n");
			goto done;
		}
	} else {
		fprintf(stderr,"Invalid command\n");
		goto done;
	}

	ret = 0;
done:
	config_cleanup(&msgq_conf);
	return ret;
}
