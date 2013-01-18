/* @@@LICENSE
*
* Copyright (c) 2012 Simon Busch <morphis@gravedo.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <pbnjson.h>
#include <luna-service2/lunaservice.h>

#include "telephonysettings.h"
#include "telephonydriver.h"
#include "telephonyservice_internal.h"
#include "utils.h"
#include "luna_service_utils.h"

void telephony_service_power_status_notify(struct telephony_service *service, bool power)
{
	jvalue_ref reply_obj = NULL;

	reply_obj = jobject_create();
	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(true));
	jobject_put(reply_obj, J_CSTR_TO_JVAL("eventPower"), jstring_create(power ? "on": "off"));

	luna_service_post_subscription(service->private_service, "/", "powerQuery", reply_obj);

	j_release(&reply_obj);
}

int _service_power_set_finish(const struct telephony_error *error, void *data)
{
	struct luna_service_req_data *req_data = data;
	struct telephony_service *service = req_data->user_data;
	jvalue_ref reply_obj = NULL;

	reply_obj = jobject_create();

	service->power_off_pending = false;

	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create((error == NULL)));

	if(!luna_service_message_validate_and_send(req_data->handle, req_data->message, reply_obj))
		luna_service_message_reply_error_internal(req_data->handle, req_data->message);

	j_release(&reply_obj);
	luna_service_req_data_free(req_data);
	return 0;
}

/**
 * @brief Set power mode for the telephony service
 *
 * JSON format:
 *    {"state":"<on|off|default>"}
 **/

bool _service_power_set_cb(LSHandle *handle, LSMessage *message, void *user_data)
{
	struct telephony_service *service = user_data;
	struct luna_service_req_data *req_data = NULL;
	bool power = false;
	jvalue_ref parsed_obj = NULL;
	jvalue_ref state_obj = NULL;
	jvalue_ref save_obj = NULL;
	const char *payload;
	const char *state_value;
	bool should_save = false;

	if (!service->initialized) {
		luna_service_message_reply_custom_error(handle, message, "Service not yet successfully initialized.");
		goto cleanup;
	}

	if (!service->driver || !service->driver->power_set) {
		g_warning("No implementation available for service powerSet API method");
		luna_service_message_reply_error_not_implemented(handle, message);
		goto cleanup;
	}

	payload = LSMessageGetPayload(message);
	parsed_obj = luna_service_message_parse_and_validate(payload);
	if (jis_null(parsed_obj)) {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	if (!jobject_get_exists(parsed_obj, J_CSTR_TO_BUF("state"), &state_obj)) {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	if (jstring_equal2(state_obj, J_CSTR_TO_BUF("on")))
		power = true;
	else if (jstring_equal2(state_obj, J_CSTR_TO_BUF("off")))
		power = false;
	else if (jstring_equal2(state_obj, J_CSTR_TO_BUF("default"))) {
		power = true;
	}
	else {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	if (jobject_get_exists(parsed_obj, J_CSTR_TO_BUF("save"), &save_obj)) {
		jboolean_get(save_obj, &should_save);
		if (should_save)
			telephony_settings_store(TELEPHONY_SETTINGS_TYPE_POWER_STATE, power ? "{\"state\":true}" : "{\"state\":false}");
	}

	service->power_off_pending = !power;

	req_data = luna_service_req_data_new(handle, message);
	req_data->user_data = service;

	if (service->driver->power_set(service, power, _service_power_set_finish, req_data) < 0) {
		g_warning("Failed to process service powerSet request in our driver");
		luna_service_message_reply_custom_error(handle, message, "Failed to set power mode");
		goto cleanup;
	}

	return true;

cleanup:
	if (!jis_null(parsed_obj))
		j_release(&parsed_obj);

	if (req_data)
		luna_service_req_data_free(req_data);

	return true;
}

int _service_power_query_finish(const struct telephony_error *error, bool power, void *data)
{
	struct luna_service_req_data *req_data = data;
	jvalue_ref reply_obj = NULL;
	jvalue_ref extended_obj = NULL;
	bool subscribed = false;
	bool success = (error == NULL);

	reply_obj = jobject_create();
	extended_obj = jobject_create();

	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(success));

	/* handle possible subscriptions */
	if (req_data->subscribed)
		jobject_put(reply_obj, J_CSTR_TO_JVAL("subscribed"), jboolean_create(req_data->subscribed));

	if (success) {
		jobject_put(extended_obj, J_CSTR_TO_JVAL("powerState"), jstring_create(power ? "on" : "off"));
		jobject_put(reply_obj, J_CSTR_TO_JVAL("extended"), extended_obj);
	}
	else {
		/* FIXME better error message */
		luna_service_message_reply_error_unknown(req_data->handle, req_data->message);
		goto cleanup;
	}

	if(!luna_service_message_validate_and_send(req_data->handle, req_data->message, reply_obj)) {
		luna_service_message_reply_error_internal(req_data->handle, req_data->message);
		goto cleanup;
	}

cleanup:
	j_release(&reply_obj);
	luna_service_req_data_free(req_data);
	return 0;
}


/**
 * @brief Query the current power status of the telephony service
 *
 * JSON format:
 *    { ["subscribe": <boolean>] }
 **/

bool _service_power_query_cb(LSHandle *handle, LSMessage *message, void *user_data)
{
	struct telephony_service *service = user_data;
	struct luna_service_req_data *req_data = NULL;
	jvalue_ref parsed_obj = NULL;
	const char *payload;

	if (!service->initialized) {
		luna_service_message_reply_custom_error(handle, message, "Service not yet successfully initialized.");
		goto cleanup;
	}

	if (!service->driver || !service->driver->power_query) {
		g_warning("No implementation available for service powerQuery API method");
		luna_service_message_reply_error_not_implemented(handle, message);
		goto cleanup;
	}

	payload = LSMessageGetPayload(message);
	parsed_obj = luna_service_message_parse_and_validate(payload);
	if (jis_null(parsed_obj)) {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	req_data = luna_service_req_data_new(handle, message);
	req_data->subscribed = luna_service_check_for_subscription_and_process(req_data->handle, req_data->message);

	if (service->driver->power_query(service, _service_power_query_finish, req_data) < 0) {
		g_warning("Failed to process service powerQuery request in our driver");
		luna_service_message_reply_custom_error(handle, message, "Failed to query power status");
		goto cleanup;
	}

	return true;

cleanup:
	if (!jis_null(parsed_obj))
		j_release(&parsed_obj);

	if (req_data)
		luna_service_req_data_free(req_data);

	return true;
}

static int _service_platform_query_finish(const struct telephony_error *error, struct telephony_platform_info *platform_info, void *data)
{
	struct luna_service_req_data *req_data = data;
	jvalue_ref reply_obj = NULL;
	jvalue_ref extended_obj = NULL;
	bool subscribed = false;
	bool success = (error == NULL);

	reply_obj = jobject_create();
	extended_obj = jobject_create();

	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(success));

	/* handle possible subscriptions */
	if (req_data->subscribed)
		jobject_put(reply_obj, J_CSTR_TO_JVAL("subscribed"), jboolean_create(req_data->subscribed));

	if (success) {
		jobject_put(extended_obj, J_CSTR_TO_JVAL("platformType"),
			jstring_create(telephony_platform_type_to_string(platform_info->platform_type)));

		if (platform_info->imei != NULL)
			jobject_put(extended_obj, J_CSTR_TO_JVAL("imei"), jstring_create(platform_info->imei));

		if (platform_info->carrier != NULL)
			jobject_put(extended_obj, J_CSTR_TO_JVAL("carrier"), jstring_create(platform_info->carrier));

		if (platform_info->mcc > 0 && platform_info->mnc > 0) {
			jobject_put(extended_obj, J_CSTR_TO_JVAL("mcc"), jnumber_create_i32(platform_info->mcc));
			jobject_put(extended_obj, J_CSTR_TO_JVAL("mnc"), jnumber_create_i32(platform_info->mnc));
		}

		if (platform_info->version != NULL)
			jobject_put(extended_obj, J_CSTR_TO_JVAL("version"), jstring_create(platform_info->version));

		jobject_put(reply_obj, J_CSTR_TO_JVAL("extended"), extended_obj);
	}
	else {
		/* FIXME better error message */
		luna_service_message_reply_error_unknown(req_data->handle, req_data->message);
		goto cleanup;
	}

	if(!luna_service_message_validate_and_send(req_data->handle, req_data->message, reply_obj)) {
		luna_service_message_reply_error_internal(req_data->handle, req_data->message);
		goto cleanup;
	}

cleanup:
	j_release(&reply_obj);
	luna_service_req_data_free(req_data);
	return 0;
}

/**
 * @brief Query various information about the platform we're running on
 **/

bool _service_platform_query_cb(LSHandle *handle, LSMessage *message, void *user_data)
{
	struct telephony_service *service = user_data;
	struct luna_service_req_data *req_data = NULL;
	jvalue_ref parsed_obj = NULL;
	const char *payload;

	if (!service->initialized) {
		luna_service_message_reply_custom_error(handle, message, "Service not yet successfully initialized.");
		goto cleanup;
	}

	if (!service->driver || !service->driver->platform_query) {
		g_warning("No implementation available for service platformQuery API method");
		luna_service_message_reply_error_not_implemented(handle, message);
		goto cleanup;
	}

	payload = LSMessageGetPayload(message);
	parsed_obj = luna_service_message_parse_and_validate(payload);
	if (jis_null(parsed_obj)) {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	req_data = luna_service_req_data_new(handle, message);
	req_data->subscribed = luna_service_check_for_subscription_and_process(req_data->handle, req_data->message);

	if (service->driver->platform_query(service, _service_platform_query_finish, req_data) < 0) {
		g_warning("Failed to process service platformQuery request in our driver");
		luna_service_message_reply_custom_error(handle, message, "Failed to query platform information");
		goto cleanup;
	}

	return true;

cleanup:
	if (!jis_null(parsed_obj))
		j_release(&parsed_obj);

	if (req_data)
		luna_service_req_data_free(req_data);

	return true;
}


// vim:ts=4:sw=4:noexpandtab