/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2019 (c) basysKom GmbH <opensource@basyskom.com> (Author: Frank Meerkötter)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "check.h"
#include "server/ua_services.h"
#include "ua_client.h"
#include "ua_types.h"
#include "ua_config_default.h"
#include "server/ua_server_internal.h"

static UA_Server *server = NULL;
static UA_ServerConfig *config = NULL;

static UA_StatusCode
methodCallback(UA_Server *serverArg,
         const UA_NodeId *sessionId, void *sessionHandle,
         const UA_NodeId *methodId, void *methodContext,
         const UA_NodeId *objectId, void *objectContext,
         size_t inputSize, const UA_Variant *input,
         size_t outputSize, UA_Variant *output)
{
    return UA_STATUSCODE_GOOD;
}

static void setup(void) {
    config = UA_ServerConfig_new_default();
    server = UA_Server_new(config);

    UA_MethodAttributes noFpAttr = UA_MethodAttributes_default;
    noFpAttr.description = UA_LOCALIZEDTEXT("en-US","No function pointer attached");
    noFpAttr.displayName = UA_LOCALIZEDTEXT("en-US","No function pointer attached");
    noFpAttr.executable = true;
    noFpAttr.userExecutable = true;
    UA_Server_addMethodNode(server, UA_NODEID_STRING(1, "nofunctionpointer"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                            UA_QUALIFIEDNAME(1, "No function pointer"),
                            noFpAttr, NULL, // no callback
                            0, NULL, 0, NULL, NULL, NULL);

    UA_MethodAttributes nonExecAttr = UA_MethodAttributes_default;
    nonExecAttr.description = UA_LOCALIZEDTEXT("en-US","Not executable");
    nonExecAttr.displayName = UA_LOCALIZEDTEXT("en-US","Not executable");
    nonExecAttr.executable = false;
    nonExecAttr.userExecutable = true;
    UA_Server_addMethodNode(server, UA_NODEID_STRING(1, "nonexec"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                            UA_QUALIFIEDNAME(1, "Not executable"),
                            nonExecAttr, &methodCallback,
                            0, NULL, 0, NULL, NULL, NULL);
}

static void teardown(void) {
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);
}

START_TEST(callUnknownMethod) {
    const UA_UInt32 UA_NS0ID_UNKNOWN_METHOD = 60000;

    UA_CallMethodRequest callMethodRequest;
    UA_CallMethodRequest_init(&callMethodRequest);
    callMethodRequest.methodId = UA_NODEID_NUMERIC(0, UA_NS0ID_UNKNOWN_METHOD);

    UA_CallMethodResult result;
    UA_CallMethodResult_init(&result);
    result = UA_Server_call(server, &callMethodRequest);
    ck_assert_int_eq(result.statusCode, UA_STATUSCODE_BADNODEIDUNKNOWN);
} END_TEST

START_TEST(callKnownMethodOnUnknownObject) {
    const UA_UInt32 UA_NS0ID_UNKNOWN_OBJECT = 60000;

    UA_CallMethodRequest callMethodRequest;
    UA_CallMethodRequest_init(&callMethodRequest);
    callMethodRequest.methodId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_REQUESTSERVERSTATECHANGE);
    callMethodRequest.objectId = UA_NODEID_NUMERIC(0, UA_NS0ID_UNKNOWN_OBJECT);

    UA_CallMethodResult result;
    UA_CallMethodResult_init(&result);
    result = UA_Server_call(server, &callMethodRequest);
    ck_assert_int_eq(result.statusCode, UA_STATUSCODE_BADNODEIDUNKNOWN);
} END_TEST

START_TEST(callMethodAndObjectExistsButMethodHasWrongNodeClass) {
    UA_CallMethodRequest callMethodRequest;
    UA_CallMethodRequest_init(&callMethodRequest);
    callMethodRequest.methodId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_AUDITING);  // not a method
    callMethodRequest.objectId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER);

    UA_CallMethodResult result;
    UA_CallMethodResult_init(&result);
    result = UA_Server_call(server, &callMethodRequest);
    ck_assert_int_eq(result.statusCode, UA_STATUSCODE_BADNODECLASSINVALID);
} END_TEST

START_TEST(callMethodAndObjectExistsButNoFunctionPointerAttached) {
    UA_CallMethodRequest callMethodRequest;
    UA_CallMethodRequest_init(&callMethodRequest);
    callMethodRequest.methodId = UA_NODEID_STRING(1, "nofunctionpointer");
    callMethodRequest.objectId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);

    UA_CallMethodResult result;
    UA_CallMethodResult_init(&result);
    result = UA_Server_call(server, &callMethodRequest);
    ck_assert_int_eq(result.statusCode, UA_STATUSCODE_BADINTERNALERROR);
} END_TEST

START_TEST(callMethodNonExecutable) {
    UA_CallMethodRequest callMethodRequest;
    UA_CallMethodRequest_init(&callMethodRequest);
    callMethodRequest.methodId = UA_NODEID_STRING(1, "nonexec");
    callMethodRequest.objectId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);

    UA_CallMethodResult result;
    UA_CallMethodResult_init(&result);
    result = UA_Server_call(server, &callMethodRequest);
    ck_assert_int_eq(result.statusCode, UA_STATUSCODE_BADNOTEXECUTABLE);
} END_TEST

int main(void) {
    Suite *s = suite_create("services_call");

    TCase *tc_call = tcase_create("call - error branches");
    tcase_add_checked_fixture(tc_call, setup, teardown);
    tcase_add_test(tc_call, callUnknownMethod);
    tcase_add_test(tc_call, callKnownMethodOnUnknownObject);
    tcase_add_test(tc_call, callMethodAndObjectExistsButMethodHasWrongNodeClass);
    tcase_add_test(tc_call, callMethodAndObjectExistsButNoFunctionPointerAttached);
    tcase_add_test(tc_call, callMethodNonExecutable);;
    suite_add_tcase(s, tc_call);

    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
