# ****************************************************************************
#    Ledger App Boilerplate
#    (c) 2020 Ledger SAS.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ****************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

include $(BOLOS_SDK)/Makefile.target

########################################
#        Mandatory configuration       #
########################################

# Enabling DEBUG flag will enable PRINTF and disable optimizations
DEBUG = 0

# Application name
APPNAME = "NEO N3"

# Application version
APPVERSION_M = 0
APPVERSION_N = 6
APPVERSION_P = 1
APPVERSION   = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

# Application source files
APP_SOURCE_PATH += src

# Application icons
ICON_STAX = icons/stax_app_neo.gif
ICON_NANOX = icons/nanox_app_neo.gif
ICON_NANOSP = icons/nanox_app_neo.gif
ICON_FLEX = icons/flex_app_neo.gif
ICON_APEX_P = icons/apex_app_neo.png

# Application allowed derivation curves.
CURVE_APP_LOAD_PARAMS = secp256r1

# Application allowed derivation paths.
PATH_APP_LOAD_PARAMS = "44'/888'"

# Setting to allow building variant applications
VARIANT_PARAM = COIN
VARIANT_VALUES = NEO3

########################################
#     Application custom permissions   #
########################################
# See SDK `include/appflags.h` for the purpose of each permission
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_NANOX TARGET_STAX TARGET_FLEX TARGET_APEX_P))
HAVE_APPLICATION_FLAG_BOLOS_SETTINGS = 1
endif

########################################
# Application communication interfaces #
########################################
ENABLE_BLUETOOTH = 1

########################################
#         NBGL custom features         #
########################################
ENABLE_NBGL_QRCODE = 1

include $(BOLOS_SDK)/Makefile.standard_app
