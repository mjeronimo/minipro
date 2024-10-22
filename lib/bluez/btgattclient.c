/**
 * @file btgattclient.c
 * @brief bluetooth main gatt client
 *
 * the original file is in bluez/tools and the name is btgatt-client.c
 *
 * @author Gilbert Brault
 * @copyright Gilbert Brault 2015
 * the original work comes from bluez v5.39
 * value add: documenting main features
 *
 */

/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2014  Google Inc.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include "l2cap.h"
#include "uuid.h"

#include "mainloop.h"
#include "util.h"
#include "att.h"
#include "queue.h"
#include "gatt-db.h"
#include "gatt-client.h"

#define ATT_CID 4

#define PRLOG(...) \
	printf(__VA_ARGS__); print_prompt();

#define COLOR_OFF	"\x1B[0m"
#define COLOR_RED	"\x1B[0;91m"
#define COLOR_GREEN	"\x1B[0;92m"
#define COLOR_YELLOW	"\x1B[0;93m"
#define COLOR_BLUE	"\x1B[0;94m"
#define COLOR_MAGENTA	"\x1B[0;95m"
#define COLOR_BOLDGRAY	"\x1B[1;30m"
#define COLOR_BOLDWHITE	"\x1B[1;36m"

static bool verbose = false;

/**
 * client structure holds gatt client context
 */
struct client {
	/// socket
	int fd;
	/// pointer to a bt_att structure
	struct bt_att *att;
	/// pointer to a gatt_db structure
	struct gatt_db *db;
	/// pointer to a bt_gatt_client structure
	struct bt_gatt_client *gatt;
	/// session id
	unsigned int reliable_session_id;
};

/**
 * print prompt
 */
static void print_prompt(void)
{
	printf(COLOR_BLUE "[GATT client]" COLOR_OFF "# ");
	fflush(stdout);
}

/**
 * convert error code to string
 *
 * @param ecode	error code (BT_ATT mostly)
 * @return 		ascii string full english ecode label
 */
static const char *ecode_to_string(uint8_t ecode)
{
	switch (ecode) {
	case BT_ATT_ERROR_INVALID_HANDLE:
		return "Invalid Handle";
	case BT_ATT_ERROR_READ_NOT_PERMITTED:
		return "Read Not Permitted";
	case BT_ATT_ERROR_WRITE_NOT_PERMITTED:
		return "Write Not Permitted";
	case BT_ATT_ERROR_INVALID_PDU:
		return "Invalid PDU";
	case BT_ATT_ERROR_AUTHENTICATION:
		return "Authentication Required";
	case BT_ATT_ERROR_REQUEST_NOT_SUPPORTED:
		return "Request Not Supported";
	case BT_ATT_ERROR_INVALID_OFFSET:
		return "Invalid Offset";
	case BT_ATT_ERROR_AUTHORIZATION:
		return "Authorization Required";
	case BT_ATT_ERROR_PREPARE_QUEUE_FULL:
		return "Prepare Write Queue Full";
	case BT_ATT_ERROR_ATTRIBUTE_NOT_FOUND:
		return "Attribute Not Found";
	case BT_ATT_ERROR_ATTRIBUTE_NOT_LONG:
		return "Attribute Not Long";
	case BT_ATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE:
		return "Insuficient Encryption Key Size";
	case BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN:
		return "Invalid Attribute value len";
	case BT_ATT_ERROR_UNLIKELY:
		return "Unlikely Error";
	case BT_ATT_ERROR_INSUFFICIENT_ENCRYPTION:
		return "Insufficient Encryption";
	case BT_ATT_ERROR_UNSUPPORTED_GROUP_TYPE:
		return "Group type Not Supported";
	case BT_ATT_ERROR_INSUFFICIENT_RESOURCES:
		return "Insufficient Resources";
	case BT_ERROR_CCC_IMPROPERLY_CONFIGURED:
		return "CCC Improperly Configured";
	case BT_ERROR_ALREADY_IN_PROGRESS:
		return "Procedure Already in Progress";
	case BT_ERROR_OUT_OF_RANGE:
		return "Out of Range";
	default:
		return "Unknown error type";
	}
}

/**
 * disconnect callback, quit mainloop
 *
 * @param err		error code associated with disconnect
 * @param user_data	user data pointer (not used)
 */
static void att_disconnect_cb(int err, void *user_data)
{
	printf("Device disconnected: %s\n", strerror(err));

	mainloop_quit();
}

/**
 * print a prefix (user_data) followed by a message (str)
 * example att: ATT op 0x02 (att: is the prefix)
 *
 * @param str		message to print
 * @param user_data pointer to prefix
 */
static void att_debug_cb(const char *str, void *user_data)
{
	const char *prefix = user_data;
	PRLOG(COLOR_BOLDGRAY "%s" COLOR_BOLDWHITE "%s\n" COLOR_OFF, prefix, str);
}

/**
 * print a prefix (user_data) followed by a message (str)
 * example gatt: MTU exchange complete, with MTU: 23 (gatt: is the prefix)
 *
 * @param str		message to print
 * @param user_data	pointer to prefix
 */
static void gatt_debug_cb(const char *str, void *user_data)
{
	const char *prefix = user_data;

	PRLOG(COLOR_GREEN "%s%s\n" COLOR_OFF, prefix, str);
}

static void ready_cb(bool success, uint8_t att_ecode, void *user_data);
static void service_changed_cb(uint16_t start_handle, uint16_t end_handle,
							void *user_data);

/**
 * log discovered service
 *
 * @param attr	gatt_db_attribute structure (service data)
 * @param str	comment about the logged service ("Service added", "Service removed"...)
 */
static void log_service_event(struct gatt_db_attribute *attr, const char *str)
{
	char uuid_str[MAX_LEN_UUID_STR];
	bt_uuid_t uuid;
	uint16_t start, end;

	gatt_db_attribute_get_service_uuid(attr, &uuid);
	bt_uuid_to_string(&uuid, uuid_str, sizeof(uuid_str));

	gatt_db_attribute_get_service_handles(attr, &start, &end);

	PRLOG("%s - UUID: %s start: 0x%04x end: 0x%04x\n", str, uuid_str,
								start, end);
}

/**
 * service added callback
 *
 * @param attr
 * @param user_data
 */
static void service_added_cb(struct gatt_db_attribute *attr, void *user_data)
{
	log_service_event(attr, "Service Added");
}

/**
 * service removed callback
 *
 * @param attr
 * @param user_data
 */
static void service_removed_cb(struct gatt_db_attribute *attr, void *user_data)
{
	log_service_event(attr, "Service Removed");
}

/**
 * create an gatt client attached to the fd l2cap socket
 *
 * @param fd	socket
 * @param mtu	selected pdu size
 * @return gatt client structure
 */
static struct client *client_create(int fd, uint16_t mtu)
{
	struct client *cli;

	cli = new0(struct client, 1);
	if (!cli) {
		fprintf(stderr, "Failed to allocate memory for client\n");
		return NULL;
	}

	cli->att = bt_att_new(fd, false);
	if (!cli->att) {
		fprintf(stderr, "Failed to initialze ATT transport layer\n");
		bt_att_unref(cli->att);
		free(cli);
		return NULL;
	}

	if (!bt_att_set_close_on_unref(cli->att, true)) {
		fprintf(stderr, "Failed to set up ATT transport layer\n");
		bt_att_unref(cli->att);
		free(cli);
		return NULL;
	}

	if (!bt_att_register_disconnect(cli->att, att_disconnect_cb, NULL,
								NULL)) {
		fprintf(stderr, "Failed to set ATT disconnect handler\n");
		bt_att_unref(cli->att);
		free(cli);
		return NULL;
	}

	cli->fd = fd;
	cli->db = gatt_db_new();
	if (!cli->db) {
		fprintf(stderr, "Failed to create GATT database\n");
		bt_att_unref(cli->att);
		free(cli);
		return NULL;
	}

	cli->gatt = bt_gatt_client_new(cli->db, cli->att, mtu);
	if (!cli->gatt) {
		fprintf(stderr, "Failed to create GATT client\n");
		gatt_db_unref(cli->db);
		bt_att_unref(cli->att);
		free(cli);
		return NULL;
	}

	gatt_db_register(cli->db, service_added_cb, service_removed_cb,
								NULL, NULL);

	if (verbose) {
		bt_att_set_debug(cli->att, att_debug_cb, "att: ", NULL);
		bt_gatt_client_set_debug(cli->gatt, gatt_debug_cb, "gatt: ",
									NULL);
	}

	bt_gatt_client_set_ready_handler(cli->gatt, ready_cb, cli, NULL);
	bt_gatt_client_set_service_changed(cli->gatt, service_changed_cb, cli,
									NULL);

	/* bt_gatt_client already holds a reference */
	gatt_db_unref(cli->db);

	return cli;
}

/**
 * client structure cleanup
 *
 * @param cli client pointer to destroy
 */
static void client_destroy(struct client *cli)
{
	bt_gatt_client_unref(cli->gatt);
	bt_att_unref(cli->att);
	free(cli);
}

/**
 * printing bt_uuid_t structure (uuid)
 *
 * @param uuid
 */
static void print_uuid(const bt_uuid_t *uuid)
{
	char uuid_str[MAX_LEN_UUID_STR];
	bt_uuid_t uuid128;

	bt_uuid_to_uuid128(uuid, &uuid128);
	bt_uuid_to_string(&uuid128, uuid_str, sizeof(uuid_str));

	printf("%s\n", uuid_str);
}

/**
 * print included data
 *
 * @param attr			gatt_db_attribute to print
 * @param user_data		client structure pointer
 */
static void print_incl(struct gatt_db_attribute *attr, void *user_data)
{
	struct client *cli = user_data;
	uint16_t handle, start, end;
	struct gatt_db_attribute *service;
	bt_uuid_t uuid;

	if (!gatt_db_attribute_get_incl_data(attr, &handle, &start, &end))
		return;

	service = gatt_db_get_attribute(cli->db, start);
	if (!service)
		return;

	gatt_db_attribute_get_service_uuid(service, &uuid);

	printf("\t  " COLOR_GREEN "include" COLOR_OFF " - handle: "
					"0x%04x, - start: 0x%04x, end: 0x%04x,"
					"uuid: ", handle, start, end);
	print_uuid(&uuid);
}

/**
 * print attribute uuid
 *
 * @param attr
 * @param user_data
 */
static void print_desc(struct gatt_db_attribute *attr, void *user_data)
{
	printf("\t\t  " COLOR_MAGENTA "descr" COLOR_OFF
					" - handle: 0x%04x, uuid: ",
					gatt_db_attribute_get_handle(attr));
	print_uuid(gatt_db_attribute_get_type(attr));
}

/**
 * print characteristic
 *
 * @param attr
 * @param user_data
 */
static void print_chrc(struct gatt_db_attribute *attr, void *user_data)
{
	uint16_t handle, value_handle;
	uint8_t properties;
	bt_uuid_t uuid;

	if (!gatt_db_attribute_get_char_data(attr, &handle,
								&value_handle,
								&properties,
								&uuid))
		return;

	printf("\t  " COLOR_YELLOW "charac" COLOR_OFF
					" - start: 0x%04x, value: 0x%04x, "
					"props: 0x%02x, uuid: ",
					handle, value_handle, properties);
	print_uuid(&uuid);

	gatt_db_service_foreach_desc(attr, print_desc, NULL);
}

/**
 * print service
 *
 * @param attr
 * @param user_data
 */
static void print_service(struct gatt_db_attribute *attr, void *user_data)
{
	struct client *cli = user_data;
	uint16_t start, end;
	bool primary;
	bt_uuid_t uuid;

	if (!gatt_db_attribute_get_service_data(attr, &start, &end, &primary,
									&uuid))
		return;

	printf(COLOR_RED "service" COLOR_OFF " - start: 0x%04x, "
				"end: 0x%04x, type: %s, uuid: ",
				start, end, primary ? "primary" : "secondary");
	print_uuid(&uuid);

	gatt_db_service_foreach_incl(attr, print_incl, cli);
	gatt_db_service_foreach_char(attr, print_chrc, NULL);

	printf("\n");
}

/**
 * print the list of services
 *
 * @param cli	client pointer
 */
static void print_services(struct client *cli)
{
	printf("\n");

	gatt_db_foreach_service(cli->db, NULL, print_service, cli);
}

/**
 * print services providing the uuid
 *
 * @param cli
 * @param uuid
 */
static void print_services_by_uuid(struct client *cli, const bt_uuid_t *uuid)
{
	printf("\n");

	gatt_db_foreach_service(cli->db, uuid, print_service, cli);
}

/**
 * print services providing the handle
 *
 * @param cli
 * @param handle
 */
static void print_services_by_handle(struct client *cli, uint16_t handle)
{
	printf("\n");

	/* TODO: Filter by handle */
	gatt_db_foreach_service(cli->db, NULL, print_service, cli);
}

/**
 * GATT discovery procedures call back
 *
 * @param success		if not 0 an error occured
 * @param att_ecode		att error code
 * @param user_data		pointer to client structure
 */
static void ready_cb(bool success, uint8_t att_ecode, void *user_data)
{
	struct client *cli = user_data;

	if (!success) {
		PRLOG("GATT discovery procedures failed - error code: 0x%02x\n",
								att_ecode);
		return;
	}

	PRLOG("GATT discovery procedures complete\n");

	print_services(cli);
	print_prompt();
}

/**
 * service changed call back
 *
 * @param start_handle
 * @param end_handle
 * @param user_data		client pointer
 */
static void service_changed_cb(uint16_t start_handle, uint16_t end_handle,
								void *user_data)
{
	struct client *cli = user_data;

	printf("\nService Changed handled - start: 0x%04x end: 0x%04x\n",
						start_handle, end_handle);

	gatt_db_foreach_service_in_range(cli->db, NULL, print_service, cli,
						start_handle, end_handle);
	print_prompt();
}

/**
 * services usage (services --help to learn options)
 */
static void services_usage(void)
{
	printf("Usage: services [options]\nOptions:\n"
		"\t -u, --uuid <uuid>\tService UUID\n"
		"\t -a, --handle <handle>\tService start handle\n"
		"\t -h, --help\t\tShow help message\n"
		"e.g.:\n"
		"\tservices\n\tservices -u 0x180d\n\tservices -a 0x0009\n");
}

/**
 * parse command string
 *
 * @param str			command to parse
 * @param expected_argc	number of arguments expected
 * @param argv			pointers to arguments separated by space or tab
 * @param argc			argument counter (and actual count)
 * @return				true = success false = error
 */
static bool parse_args(char *str, int expected_argc,  char **argv, int *argc)
{
	char **ap;

	for (ap = argv; (*ap = strsep(&str, " \t")) != NULL;) {
		if (**ap == '\0')
			continue;

		(*argc)++;
		ap++;

		if (*argc > expected_argc)
			return false;
	}

	return true;
}

/**
 * command interpreter
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	one of the possible command (help to get a list)
 * 					&lt;command&gt; --help to have a help on that command
 */
static void cmd_services(struct client *cli, char *cmd_str)
{
	char *argv[3];
	int argc = 0;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 2, argv, &argc)) {
		services_usage();
		return;
	}

	if (!argc) {
		print_services(cli);
		return;
	}

	if (argc != 2) {
		services_usage();
		return;
	}

	if (!strcmp(argv[0], "-u") || !strcmp(argv[0], "--uuid")) {
		bt_uuid_t tmp, uuid;

		if (bt_string_to_uuid(&tmp, argv[1]) < 0) {
			printf("Invalid UUID: %s\n", argv[1]);
			return;
		}

		bt_uuid_to_uuid128(&tmp, &uuid);

		print_services_by_uuid(cli, &uuid);
	} else if (!strcmp(argv[0], "-a") || !strcmp(argv[0], "--handle")) {
		uint16_t handle;
		char *endptr = NULL;

		handle = strtol(argv[1], &endptr, 0);
		if (!endptr || *endptr != '\0') {
			printf("Invalid start handle: %s\n", argv[1]);
			return;
		}

		print_services_by_handle(cli, handle);
	} else
		services_usage();
}

/**
 * read multiple usage
 */
static void read_multiple_usage(void)
{
	printf("Usage: read-multiple <handle_1> <handle_2> ...\n");
}

/**
 * read multiple callback
 *
 * @param success		==0 => print error, <>0 print values
 * @param att_ecode 	att error code
 * @param value			vector of values
 * @param length		number of values
 * @param user_data		not used
 */
static void read_multiple_cb(bool success, uint8_t att_ecode,
					const uint8_t *value, uint16_t length,
					void *user_data)
{
	int i;

	if (!success) {
		PRLOG("\nRead multiple request failed: 0x%02x\n", att_ecode);
		return;
	}

	printf("\nRead multiple value (%u bytes):", length);

	for (i = 0; i < length; i++)
		printf("%02x ", value[i]);

	PRLOG("\n");
}

/**
 * read multiple command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	command string for read multiple
 */
static void cmd_read_multiple(struct client *cli, char *cmd_str)
{
	int argc = 0;
	uint16_t *value;
	char *argv[512];
	int i;
	char *endptr = NULL;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, sizeof(argv), argv, &argc) || argc < 2) {
		read_multiple_usage();
		return;
	}

	value = malloc(sizeof(uint16_t) * argc);
	if (!value) {
		printf("Failed to construct value\n");
		return;
	}

	for (i = 0; i < argc; i++) {
		value[i] = strtol(argv[i], &endptr, 0);
		if (endptr == argv[i] || *endptr != '\0' || !value[i]) {
			printf("Invalid value byte: %s\n", argv[i]);
			free(value);
			return;
		}
	}

	if (!bt_gatt_client_read_multiple(cli->gatt, value, argc,
						read_multiple_cb, NULL, NULL))
		printf("Failed to initiate read multiple procedure\n");

	free(value);
}

/**
 * read value usage
 */
static void read_value_usage(void)
{
	printf("Usage: read-value <value_handle>\n");
}

/**
 * read value callback
 *
 * @param success		==0 => print error, <>0 print value
 * @param att_ecode		att error code
 * @param value			vector of values
 * @param length		size of vector
 * @param user_data		not used
 */
static void read_cb(bool success, uint8_t att_ecode, const uint8_t *value,
					uint16_t length, void *user_data)
{
	int i;

	if (!success) {
		PRLOG("\nRead request failed: %s (0x%02x)\n",
				ecode_to_string(att_ecode), att_ecode);
		return;
	}

	printf("\nRead value");

	if (length == 0) {
		PRLOG(": 0 bytes\n");
		return;
	}

	printf(" (%u bytes): ", length);

	for (i = 0; i < length; i++)
		printf("%02x ", value[i]);

	PRLOG("\n");
}

/**
 * read value command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	command string for read value
 */
static void cmd_read_value(struct client *cli, char *cmd_str)
{
	char *argv[2];
	int argc = 0;
	uint16_t handle;
	char *endptr = NULL;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 1, argv, &argc) || argc != 1) {
		read_value_usage();
		return;
	}

	handle = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !handle) {
		printf("Invalid value handle: %s\n", argv[0]);
		return;
	}

	if (!bt_gatt_client_read_value(cli->gatt, handle, read_cb,
								NULL, NULL))
		printf("Failed to initiate read value procedure\n");
}

/**
 *  read long value usage
 */
static void read_long_value_usage(void)
{
	printf("Usage: read-long-value <value_handle> <offset>\n");
}

/**
 * read long value command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	command string for read long value
 */
static void cmd_read_long_value(struct client *cli, char *cmd_str)
{
	char *argv[3];
	int argc = 0;
	uint16_t handle;
	uint16_t offset;
	char *endptr = NULL;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 2, argv, &argc) || argc != 2) {
		read_long_value_usage();
		return;
	}

	handle = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !handle) {
		printf("Invalid value handle: %s\n", argv[0]);
		return;
	}

	endptr = NULL;
	offset = strtol(argv[1], &endptr, 0);
	if (!endptr || *endptr != '\0') {
		printf("Invalid offset: %s\n", argv[1]);
		return;
	}

	if (!bt_gatt_client_read_long_value(cli->gatt, handle, offset, read_cb,
								NULL, NULL))
		printf("Failed to initiate read long value procedure\n");
}

/**
 * write value usage
 */
static void write_value_usage(void)
{
	printf("Usage: write-value [options] <value_handle> <value>\n"
		"Options:\n"
		"\t-w, --without-response\tWrite without response\n"
		"\t-s, --signed-write\tSigned write command\n"
		"e.g.:\n"
		"\twrite-value 0x0001 00 01 00\n");
}

static struct option write_value_options[] = {
	{ "without-response",	0, 0, 'w' },
	{ "signed-write",	0, 0, 's' },
	{ }
};

/**
 * write value call back
 *
 * @param success		==0 => print error, <>0 print value
 * @param att_ecode		att error code
 * @param user_data		not used
 */
static void write_cb(bool success, uint8_t att_ecode, void *user_data)
{
	if (success) {
		PRLOG("\nWrite successful\n");
	} else {
		PRLOG("\nWrite failed: %s (0x%02x)\n",
				ecode_to_string(att_ecode), att_ecode);
	}
}

/**
 * write value command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	command string for write value
 */
static void cmd_write_value(struct client *cli, char *cmd_str)
{
	int opt, i;
	char *argvbuf[516];
	char **argv = argvbuf;
	int argc = 1;
	uint16_t handle;
	char *endptr = NULL;
	int length;
	uint8_t *value = NULL;
	bool without_response = false;
	bool signed_write = false;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 514, argv + 1, &argc)) {
		printf("Too many arguments\n");
		write_value_usage();
		return;
	}

	optind = 0;
	argv[0] = "write-value";
	while ((opt = getopt_long(argc, argv, "+ws", write_value_options,
								NULL)) != -1) {
		switch (opt) {
		case 'w':
			without_response = true;
			break;
		case 's':
			signed_write = true;
			break;
		default:
			write_value_usage();
			return;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		write_value_usage();
		return;
	}

	handle = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !handle) {
		printf("Invalid handle: %s\n", argv[0]);
		return;
	}

	length = argc - 1;

	if (length > 0) {
		if (length > UINT16_MAX) {
			printf("Write value too long\n");
			return;
		}

		value = malloc(length);
		if (!value) {
			printf("Failed to construct write value\n");
			return;
		}

		for (i = 1; i < argc; i++) {
#if 0
			if (strlen(argv[i]) != 2) {
				printf("Invalid value byte: %s\n",
								argv[i]);
				goto done;
			}

			value[i-1] = strtol(argv[i], &endptr, 0);
#endif
			value[i-1] = strtol(argv[i], &endptr, 16);
			if (endptr == argv[i] || *endptr != '\0'
							|| errno == ERANGE) {
				printf("Invalid value byte: %s\n",
								argv[i]);
				goto done;
			}
		}
	}

	if (without_response) {
		if (!bt_gatt_client_write_without_response(cli->gatt, handle,
						signed_write, value, length)) {
			printf("Failed to initiate write without response "
								"procedure\n");
			goto done;
		}

		printf("Write command sent\n");
		goto done;
	}

	if (!bt_gatt_client_write_value(cli->gatt, handle, value, length,
								write_cb,
								NULL, NULL))
		printf("Failed to initiate write procedure\n");

done:
	free(value);
}

/**
 * write long value usage
 */
static void write_long_value_usage(void)
{
	printf("Usage: write-long-value [options] <value_handle> <offset> "
				"<value>\n"
				"Options:\n"
				"\t-r, --reliable-write\tReliable write\n"
				"e.g.:\n"
				"\twrite-long-value 0x0001 0 00 01 00\n");
}

static struct option write_long_value_options[] = {
	{ "reliable-write",	0, 0, 'r' },
	{ }
};

/**
 * write long call back
 *
 * @param success			==0 => print error, <>0 print value
 * @param reliable_error	status of the write operation when failed
 * @param att_ecode			att error code
 * @param user_data			not used
 */
static void write_long_cb(bool success, bool reliable_error, uint8_t att_ecode,
								void *user_data)
{
	if (success) {
		PRLOG("Write successful\n");
	} else if (reliable_error) {
		PRLOG("Reliable write not verified\n");
	} else {
		PRLOG("\nWrite failed: %s (0x%02x)\n",
				ecode_to_string(att_ecode), att_ecode);
	}
}

/**
 * write long value command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	command string for write long value
 */
static void cmd_write_long_value(struct client *cli, char *cmd_str)
{
	int opt, i;
	char *argvbuf[516];
	char **argv = argvbuf;
	int argc = 1;
	uint16_t handle;
	uint16_t offset;
	char *endptr = NULL;
	int length;
	uint8_t *value = NULL;
	bool reliable_writes = false;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 514, argv + 1, &argc)) {
		printf("Too many arguments\n");
		write_value_usage();
		return;
	}

	optind = 0;
	argv[0] = "write-long-value";
	while ((opt = getopt_long(argc, argv, "+r", write_long_value_options,
								NULL)) != -1) {
		switch (opt) {
		case 'r':
			reliable_writes = true;
			break;
		default:
			write_long_value_usage();
			return;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2) {
		write_long_value_usage();
		return;
	}

	handle = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !handle) {
		printf("Invalid handle: %s\n", argv[0]);
		return;
	}

	endptr = NULL;
	offset = strtol(argv[1], &endptr, 0);
	if (!endptr || *endptr != '\0' || errno == ERANGE) {
		printf("Invalid offset: %s\n", argv[1]);
		return;
	}

	length = argc - 2;

	if (length > 0) {
		if (length > UINT16_MAX) {
			printf("Write value too long\n");
			return;
		}

		value = malloc(length);
		if (!value) {
			printf("Failed to construct write value\n");
			return;
		}

		for (i = 2; i < argc; i++) {
			if (strlen(argv[i]) != 2) {
				printf("Invalid value byte: %s\n",
								argv[i]);
				free(value);
				return;
			}

			value[i-2] = strtol(argv[i], &endptr, 0);
			if (endptr == argv[i] || *endptr != '\0'
							|| errno == ERANGE) {
				printf("Invalid value byte: %s\n",
								argv[i]);
				free(value);
				return;
			}
		}
	}

	if (!bt_gatt_client_write_long_value(cli->gatt, reliable_writes, handle,
							offset, value, length,
							write_long_cb,
							NULL, NULL))
		printf("Failed to initiate long write procedure\n");

	free(value);
}

/**
 * write prepare usage
 */
static void write_prepare_usage(void)
{
	printf("Usage: write-prepare [options] <value_handle> <offset> "
				"<value>\n"
				"Options:\n"
				"\t-s, --session-id\tSession id\n"
				"e.g.:\n"
				"\twrite-prepare -s 1 0x0001 00 01 00\n");
}

static struct option write_prepare_options[] = {
	{ "session-id",		1, 0, 's' },
	{ }
};

/**
 * write prepare command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	write prepare command string
 */
static void cmd_write_prepare(struct client *cli, char *cmd_str)
{
	int opt, i;
	char *argvbuf[516];
	char **argv = argvbuf;
	int argc = 0;
	unsigned int id = 0;
	uint16_t handle;
	uint16_t offset;
	char *endptr = NULL;
	unsigned int length;
	uint8_t *value = NULL;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 514, argv + 1, &argc)) {
		printf("Too many arguments\n");
		write_value_usage();
		return;
	}

	/* Add command name for getopt_long */
	argc++;
	argv[0] = "write-prepare";

	optind = 0;
	while ((opt = getopt_long(argc, argv , "s:", write_prepare_options,
								NULL)) != -1) {
		switch (opt) {
		case 's':
			if (!optarg) {
				write_prepare_usage();
				return;
			}

			id = atoi(optarg);

			break;
		default:
			write_prepare_usage();
			return;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 3) {
		write_prepare_usage();
		return;
	}

	if (cli->reliable_session_id != id) {
		printf("Session id != Ongoing session id (%u!=%u)\n", id,
						cli->reliable_session_id);
		return;
	}

	handle = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !handle) {
		printf("Invalid handle: %s\n", argv[0]);
		return;
	}

	endptr = NULL;
	offset = strtol(argv[1], &endptr, 0);
	if (!endptr || *endptr != '\0' || errno == ERANGE) {
		printf("Invalid offset: %s\n", argv[1]);
		return;
	}

	/*
	 * First two arguments are handle and offset. What remains is the value
	 * length
	 */
	length = argc - 2;

	if (length == 0)
		goto done;

	if (length > UINT16_MAX) {
		printf("Write value too long\n");
		return;
	}

	value = malloc(length);
	if (!value) {
		printf("Failed to allocate memory for value\n");
		return;
	}

	for (i = 2; i < argc; i++) {
		if (strlen(argv[i]) != 2) {
			printf("Invalid value byte: %s\n", argv[i]);
			free(value);
			return;
		}

		value[i-2] = strtol(argv[i], &endptr, 0);
		if (endptr == argv[i] || *endptr != '\0' || errno == ERANGE) {
			printf("Invalid value byte: %s\n", argv[i]);
			free(value);
			return;
		}
	}

done:
	cli->reliable_session_id =
			bt_gatt_client_prepare_write(cli->gatt, id,
							handle, offset,
							value, length,
							write_long_cb, NULL,
							NULL);
	if (!cli->reliable_session_id)
		printf("Failed to proceed prepare write\n");
	else
		printf("Prepare write success.\n"
				"Session id: %d to be used on next write\n",
						cli->reliable_session_id);

	free(value);
}

/**
 * write execute usage
 */
static void write_execute_usage(void)
{
	printf("Usage: write-execute <session_id> <execute>\n"
				"e.g.:\n"
				"\twrite-execute 1 0\n");
}

/**
 * write execute command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	write execute command string
 */
static void cmd_write_execute(struct client *cli, char *cmd_str)
{
	char *argvbuf[516];
	char **argv = argvbuf;
	int argc = 0;
	char *endptr = NULL;
	unsigned int session_id;
	bool execute;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 514, argv, &argc)) {
		printf("Too many arguments\n");
		write_value_usage();
		return;
	}

	if (argc < 2) {
		write_execute_usage();
		return;
	}

	session_id = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0') {
		printf("Invalid session id: %s\n", argv[0]);
		return;
	}

	if (session_id != cli->reliable_session_id) {
		printf("Invalid session id: %u != %u\n", session_id,
						cli->reliable_session_id);
		return;
	}

	execute = !!strtol(argv[1], &endptr, 0);
	if (!endptr || *endptr != '\0') {
		printf("Invalid execute: %s\n", argv[1]);
		return;
	}

	if (execute) {
		if (!bt_gatt_client_write_execute(cli->gatt, session_id,
							write_cb, NULL, NULL))
			printf("Failed to proceed write execute\n");
	} else {
		bt_gatt_client_cancel(cli->gatt, session_id);
	}

	cli->reliable_session_id = 0;
}

/**
 * register notify usage
 */
static void register_notify_usage(void)
{
	printf("Usage: register-notify <chrc value handle>\n");
}

/**
 * notify call back
 *
 * @param value_handle	handle of the notifying object
 * @param value			vector value of the object
 * @param length		length of vector value
 * @param user_data		not used
 */
static void notify_cb(uint16_t value_handle, const uint8_t *value,
					uint16_t length, void *user_data)
{
	int i;

	printf("\n\tHandle Value Not/Ind: 0x%04x - ", value_handle);

	if (length == 0) {
		PRLOG("(0 bytes)\n");
		return;
	}

	printf("(%u bytes): ", length);

	for (i = 0; i < length; i++)
		printf("%02x ", value[i]);

	PRLOG("\n");
}

/**
 *  register notify call back
 *
 * @param att_ecode		att error code
 * @param user_data		not used
 */
static void register_notify_cb(uint16_t att_ecode, void *user_data)
{
	if (att_ecode) {
		PRLOG("Failed to register notify handler "
					"- error code: 0x%02x\n", att_ecode);
		return;
	}

	PRLOG("Registered notify handler!");
}

/**
 * register notify command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	register notify command string
 */
static void cmd_register_notify(struct client *cli, char *cmd_str)
{
	char *argv[2];
	int argc = 0;
	uint16_t value_handle;
	unsigned int id;
	char *endptr = NULL;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 1, argv, &argc) || argc != 1) {
		register_notify_usage();
		return;
	}

	value_handle = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !value_handle) {
		printf("Invalid value handle: %s\n", argv[0]);
		return;
	}

	id = bt_gatt_client_register_notify(cli->gatt, value_handle,
							register_notify_cb,
							notify_cb, NULL, NULL);
	if (!id) {
		printf("Failed to register notify handler\n");
		return;
	}

	PRLOG("Registering notify handler with id: %u\n", id);
}

/**
 * un-register notify usage
 */
static void unregister_notify_usage(void)
{
	printf("Usage: unregister-notify <notify id>\n");
}

/**
 * un-register notify command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	un-register notify command string
 */
static void cmd_unregister_notify(struct client *cli, char *cmd_str)
{
	char *argv[2];
	int argc = 0;
	unsigned int id;
	char *endptr = NULL;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 1, argv, &argc) || argc != 1) {
		unregister_notify_usage();
		return;
	}

	id = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || !id) {
		printf("Invalid notify id: %s\n", argv[0]);
		return;
	}

	if (!bt_gatt_client_unregister_notify(cli->gatt, id)) {
		printf("Failed to unregister notify handler with id: %u\n", id);
		return;
	}

	printf("Unregistered notify handler with id: %u\n", id);
}

/**
 * set security usage
 */
static void set_security_usage(void)
{
	printf("Usage: set_security <level>\n"
		"level: 1-3\n"
		"e.g.:\n"
		"\tset-sec-level 2\n");
}

/**
 * set security command
 *
 * @param cli		pointer to the client structure
 * @param cmd_str	set security command string
 */
static void cmd_set_security(struct client *cli, char *cmd_str)
{
	char *argvbuf[1];
	char **argv = argvbuf;
	int argc = 0;
	char *endptr = NULL;
	int level;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	if (!parse_args(cmd_str, 1, argv, &argc)) {
		printf("Too many arguments\n");
		set_security_usage();
		return;
	}

	if (argc < 1) {
		set_security_usage();
		return;
	}

	level = strtol(argv[0], &endptr, 0);
	if (!endptr || *endptr != '\0' || level < 1 || level > 3) {
		printf("Invalid level: %s\n", argv[0]);
		return;
	}

	if (!bt_gatt_client_set_security(cli->gatt, level))
		printf("Could not set sec level\n");
	else
		printf("Setting security level %d success\n", level);
}

/**
 * get security command
 *
 * @param cli		pointer to the client stricture
 * @param cmd_str	get security command string
 */
static void cmd_get_security(struct client *cli, char *cmd_str)
{
	int level;

	if (!bt_gatt_client_is_ready(cli->gatt)) {
		printf("GATT client not initialized\n");
		return;
	}

	level = bt_gatt_client_get_security(cli->gatt);
	if (level < 0)
		printf("Could not set sec level\n");
	else
		printf("Security level: %u\n", level);
}

/**
 * convert sign key ascii to vector
 *
 * @param optarg	ascii key string
 * @param key		returned parsed vector
 * @return 			true if success else false
 */
static bool convert_sign_key(char *optarg, uint8_t key[16])
{
	int i;

	if (strlen(optarg) != 32) {
		printf("sign-key length is invalid\n");
		return false;
	}

	for (i = 0; i < 16; i++) {
		if (sscanf(optarg + (i * 2), "%2hhx", &key[i]) != 1)
			return false;
	}

	return true;
}

/**
 * sign key usage
 */
static void set_sign_key_usage(void)
{
	printf("Usage: set-sign-key [options]\nOptions:\n"
		"\t -c, --sign-key <csrk>\tCSRK\n"
		"e.g.:\n"
		"\tset-sign-key -c D8515948451FEA320DC05A2E88308188\n");
}

/**
 * counter increment sign_cnt by one
 *
 * @param sign_cnt		variable to increment
 * @param user_data		not used
 * @return				true
 */
static bool local_counter(uint32_t *sign_cnt, void *user_data)
{
	static uint32_t cnt = 0;

	*sign_cnt = cnt++;

	return true;
}

/**
 * set sign key command
 *
 * @param cli		pointer to client structure
 * @param cmd_str	set sign key command string
 */
static void cmd_set_sign_key(struct client *cli, char *cmd_str)
{
	char *argv[3];
	int argc = 0;
	uint8_t key[16];

	memset(key, 0, 16);

	if (!parse_args(cmd_str, 2, argv, &argc)) {
		set_sign_key_usage();
		return;
	}

	if (argc != 2) {
		set_sign_key_usage();
		return;
	}

	if (!strcmp(argv[0], "-c") || !strcmp(argv[0], "--sign-key")) {
		if (convert_sign_key(argv[1], key))
			bt_att_set_local_key(cli->att, key, local_counter, cli);
	} else
		set_sign_key_usage();
}

static void cmd_help(struct client *cli, char *cmd_str);

static void cmd_quit(struct client *cli, char *cmd_str){
	mainloop_quit();
}


typedef void (*command_func_t)(struct client *cli, char *cmd_str);

static struct {
	char *cmd;
	command_func_t func;
	char *doc;
} command[] = {
	{ "help", cmd_help, "\tDisplay help message" },
	{ "services", cmd_services, "\tShow discovered services" },
	{ "read-value", cmd_read_value,
				"\tRead a characteristic or descriptor value" },
	{ "read-long-value", cmd_read_long_value,
		"\tRead a long characteristic or desctriptor value" },
	{ "read-multiple", cmd_read_multiple, "\tRead Multiple" },
	{ "write-value", cmd_write_value,
			"\tWrite a characteristic or descriptor value" },
	{ "write-long-value", cmd_write_long_value,
			"Write long characteristic or descriptor value" },
	{ "write-prepare", cmd_write_prepare,
			"\tWrite prepare characteristic or descriptor value" },
	{ "write-execute", cmd_write_execute,
			"\tExecute already prepared write" },
	{ "register-notify", cmd_register_notify,
			"\tSubscribe to not/ind from a characteristic" },
	{ "unregister-notify", cmd_unregister_notify,
						"Unregister a not/ind session"},
	{ "set-security", cmd_set_security,
				"\tSet security level on le connection"},
	{ "get-security", cmd_get_security,
				"\tGet security level on le connection"},
	{ "set-sign-key", cmd_set_sign_key,
				"\tSet signing key for signed write command"},
	{ "quit", cmd_quit ,"\tQuit"},
	{ }
};

/**
 * help command
 *
 * @param cli		pointer to client structure
 * @param cmd_str	help command string
 */
static void cmd_help(struct client *cli, char *cmd_str)
{
	int i;

	printf("Commands:\n");
	for (i = 0; command[i].cmd; i++)
		printf("\t%-15s\t%s\n", command[i].cmd, command[i].doc);
}

/**
 * prompt read call back
 *
 * @param fd		stdin
 * @param events	epoll event
 * @param user_data pointer to client structure
 */
static void prompt_read_cb(int fd, uint32_t events, void *user_data)
{
	ssize_t read;
	size_t len = 0;
	char *line = NULL;
	char *cmd = NULL, *args;
	struct client *cli = user_data;
	int i;

	if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
		mainloop_quit();
		return;
	}

	if ((read = getline(&line, &len, stdin)) == -1)
		return;

	if (read <= 1) {
		cmd_help(cli, NULL);
		print_prompt();
		return;
	}

	line[read-1] = '\0';
	args = line;

	while ((cmd = strsep(&args, " \t")))
		if (*cmd != '\0')
			break;

	if (!cmd)
		goto failed;

	for (i = 0; command[i].cmd; i++) {
		if (strcmp(command[i].cmd, cmd) == 0)
			break;
	}

	if (command[i].cmd)
		command[i].func(cli, args);
	else
		fprintf(stderr, "Unknown command: %s\n", line);

failed:
	print_prompt();

	free(line);
}

/**
 * signal call back SIGINT and SIGTERM processing
 *
 * @param signum		SIGXXX
 * @param user_data		unused
 */
static void signal_cb(int signum, void *user_data)
{
	switch (signum) {
	case SIGINT:
	case SIGTERM:
		mainloop_quit();
		break;
	default:
		break;
	}
}

/**
 * create a bluetooth le l2cap socket and connect src to dst
 *
 * @param src	6 bytes BD Address source address
 * @param dst	6 bytes BD Address destination address
 * @param dst_type  destination type BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM
 * @param sec   security level BT_SECURITY_LOW or BT_SECURITY_MEDIUM or BT_SECURITY_HIGH
 * @return socket or -1 if error
 */
static int l2cap_le_att_connect(bdaddr_t *src, bdaddr_t *dst, uint8_t dst_type,
									int sec)
{
	int sock;
	struct sockaddr_l2 srcaddr, dstaddr;
	struct bt_security btsec;

	if (verbose) {
		char srcaddr_str[18], dstaddr_str[18];

		ba2str(src, srcaddr_str);
		ba2str(dst, dstaddr_str);

		printf("btgatt-client: Opening L2CAP LE connection on ATT "
					"channel:\n\t src: %s\n\tdest: %s\n",
					srcaddr_str, dstaddr_str);
	}

	sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (sock < 0) {
		perror("Failed to create L2CAP socket");
		return -1;
	}

	/* Set up source address */
	memset(&srcaddr, 0, sizeof(srcaddr));
	srcaddr.l2_family = AF_BLUETOOTH;
	srcaddr.l2_cid = htobs(ATT_CID);
	srcaddr.l2_bdaddr_type = 0;
	bacpy(&srcaddr.l2_bdaddr, src);

	if (bind(sock, (struct sockaddr *)&srcaddr, sizeof(srcaddr)) < 0) {
		perror("Failed to bind L2CAP socket");
		close(sock);
		return -1;
	}

	/* Set the security level */
	memset(&btsec, 0, sizeof(btsec));
	btsec.level = sec;
	if (setsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &btsec,
							sizeof(btsec)) != 0) {
		fprintf(stderr, "Failed to set L2CAP security level\n");
		close(sock);
		return -1;
	}

	/* Set up destination address */
	memset(&dstaddr, 0, sizeof(dstaddr));
	dstaddr.l2_family = AF_BLUETOOTH;
	dstaddr.l2_cid = htobs(ATT_CID);
	dstaddr.l2_bdaddr_type = dst_type;
	bacpy(&dstaddr.l2_bdaddr, dst);

	printf("Connecting to device...");
	fflush(stdout);

	if (connect(sock, (struct sockaddr *) &dstaddr, sizeof(dstaddr)) < 0) {
		perror(" Failed to connect");
		close(sock);
		return -1;
	}

	printf(" Done\n");

	return sock;
}

/**
 * print usage
 */
static void usage(void)
{
	printf("btgatt-client\n");
	printf("Usage:\n\tbtgatt-client [options]\n");

	printf("Options:\n"
		"\t-i, --index <id>\t\tSpecify adapter index, e.g. hci0\n"
		"\t-d, --dest <addr>\t\tSpecify the destination address\n"
		"\t-t, --type [random|public] \tSpecify the LE address type\n"
		"\t-m, --mtu <mtu> \t\tThe ATT MTU to use\n"
		"\t-s, --security-level <sec> \tSet security level (low|"
								"medium|high)\n"
		"\t-v, --verbose\t\t\tEnable extra logging\n"
		"\t-h, --help\t\t\tDisplay help\n");

	printf("Example:\n"
			"btgattclient -v -d C4:BE:84:70:29:04\n");
}

static struct option main_options[] = {
	{ "index",		1, 0, 'i' },
	{ "dest",		1, 0, 'd' },
	{ "type",		1, 0, 't' },
	{ "mtu",		1, 0, 'm' },
	{ "security-level",	1, 0, 's' },
	{ "verbose",		0, 0, 'v' },
	{ "help",		0, 0, 'h' },
	{ }
};

/**
 * gatt client which browse ble device services and characteristics
 * @see usage function for args definition
 *
 * @param argc	args count
 * @param argv	args value
 * @return EXIT_FAILURE or EXIT_SUCCESS
 */
int main(int argc, char *argv[])
{
	int opt;
	int sec = BT_SECURITY_LOW;
	uint16_t mtu = 0;
	uint8_t dst_type = BDADDR_LE_PUBLIC;
	bool dst_addr_given = false;
	bdaddr_t src_addr, dst_addr;
	int dev_id = -1;
	int fd;
	sigset_t mask;
	struct client *cli;

	while ((opt = getopt_long(argc, argv, "+hvs:m:t:d:i:",
						main_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 'v':
			verbose = true;
			break;
		case 's':
			if (strcmp(optarg, "low") == 0)
				sec = BT_SECURITY_LOW;
			else if (strcmp(optarg, "medium") == 0)
				sec = BT_SECURITY_MEDIUM;
			else if (strcmp(optarg, "high") == 0)
				sec = BT_SECURITY_HIGH;
			else {
				fprintf(stderr, "Invalid security level\n");
				return EXIT_FAILURE;
			}
			break;
		case 'm': {
			int arg;

			arg = atoi(optarg);
			if (arg <= 0) {
				fprintf(stderr, "Invalid MTU: %d\n", arg);
				return EXIT_FAILURE;
			}

			if (arg > UINT16_MAX) {
				fprintf(stderr, "MTU too large: %d\n", arg);
				return EXIT_FAILURE;
			}

			mtu = (uint16_t)arg;
			break;
		}
		case 't':
			if (strcmp(optarg, "random") == 0)
				dst_type = BDADDR_LE_RANDOM;
			else if (strcmp(optarg, "public") == 0)
				dst_type = BDADDR_LE_PUBLIC;
			else {
				fprintf(stderr,
					"Allowed types: random, public\n");
				return EXIT_FAILURE;
			}
			break;
		case 'd':
			if (str2ba(optarg, &dst_addr) < 0) {
				fprintf(stderr, "Invalid remote address: %s\n",
									optarg);
				return EXIT_FAILURE;
			}

			dst_addr_given = true;
			break;

		case 'i':
			dev_id = hci_devid(optarg);
			if (dev_id < 0) {
				perror("Invalid adapter");
				return EXIT_FAILURE;
			}

			break;
		default:
			fprintf(stderr, "Invalid option: %c\n", opt);
			return EXIT_FAILURE;
		}
	}

	if (!argc) {
		usage();
		return EXIT_SUCCESS;
	}

	argc -= optind;
	argv += optind;
	optind = 0;

	if (argc) {
		usage();
		return EXIT_SUCCESS;
	}

	if (dev_id == -1)
		bacpy(&src_addr, BDADDR_ANY);
	else if (hci_devba(dev_id, &src_addr) < 0) {
		perror("Adapter not available");
		return EXIT_FAILURE;
	}

	if (!dst_addr_given) {
		fprintf(stderr, "Destination address required!\n");
		return EXIT_FAILURE;
	}

	/* create the mainloop resources */
	mainloop_init();

	fd = l2cap_le_att_connect(&src_addr, &dst_addr, dst_type, sec);
	if (fd < 0)
		return EXIT_FAILURE;

	cli = client_create(fd, mtu);
	if (!cli) {
		close(fd);
		return EXIT_FAILURE;
	}

	/* add input event from console */
	if (mainloop_add_fd(fileno(stdin),
				EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
				prompt_read_cb, cli, NULL) < 0) {
		fprintf(stderr, "Failed to initialize console\n");
		return EXIT_FAILURE;
	}

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	/* add handler for process interrupted (SIGINT) or terminated (SIGTERM)*/
	mainloop_set_signal(&mask, signal_cb, NULL, NULL);

	print_prompt();

	/* epoll main loop call
	 *
	 * any further process is an epoll event processed in mainloop_run
	 *
	 */
	mainloop_run();

	printf("\n\nShutting down...\n");

	client_destroy(cli);

	return EXIT_SUCCESS;
}

