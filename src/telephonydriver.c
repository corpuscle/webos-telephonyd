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

#include <glib.h>
#include <stdbool.h>
#include "telephonydriver.h"

const char* telephony_platform_type_to_string(enum telephony_platform_type type)
{
	switch (type) {
		case TELEPHONY_PLATFORM_TYPE_GSM:
			return "gsm";
		case TELEPHONY_PLATFORM_TYPE_CDMA:
			return "cdma";
	}
	return NULL;
}
