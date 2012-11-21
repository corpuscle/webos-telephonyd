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

#ifndef LUNA_SERVICE_UTILS_H_
#define LUNA_SERVICE_UTILS_H_

#include <luna-service2/lunaservice.h>

void luna_service_message_reply_custom_error(LSHandle *handle, LSMessage *message, const char *error_text);
void luna_service_message_reply_error_unknown(LSHandle *handle, LSMessage *message);
void luna_service_message_reply_error_bad_json(LSHandle *handle, LSMessage *message);
void luna_service_message_reply_error_invalid_params(LSHandle *handle, LSMessage *message);
void luna_service_message_reply_error_not_implemented(LSHandle *handle, LSMessage *message);
void luna_service_message_reply_error_internal(LSHandle *handle, LSMessage *message);
void luna_service_message_reply_success(LSHandle *handle, LSMessage *message);

#endif

// vim:ts=4:sw=4:noexpandtab
