// Copyright 2020 Charles Tytler

#include "../Windows/pch.h"
#include "gtest/gtest.h"
#include <unordered_map>

// Create mock version of ESDConnectionManager for testing.
#define UNIT_TEST
const int kESDSDKTarget_HardwareAndSoftware = 0;

class ESDConnectionManager {
  public:
    void SetState(int state, std::string context) {
        context_ = context;
        state_ = state;
    }
    void SetTitle(std::string title, std::string context, int flag) {
        context_ = context;
        title_ = title;
    }

    // Only in mock class:
    void clear_buffer() {
        context_ = "";
        state_ = 0;
        title_ = "";
    }

    std::string context_ = "";
    int state_ = 0;
    std::string title_ = "";
};

#include "../StreamdeckContext.cpp"

class StreamdeckContextTestFixture : public ::testing::Test {
  public:
    StreamdeckContextTestFixture()
        : dcs_interface(kDcsListenerPort, kDcsSendPort, kDcsIpAddress),
          mock_dcs(kDcsSendPort, kDcsListenerPort, kDcsIpAddress), fixture_context(fixture_context_id) {}

    DcsInterface dcs_interface;                  // DCS Interface to test.
    DcsSocket mock_dcs;                          // A socket that will mock Send/Receive messages from DCS.
    ESDConnectionManager esd_connection_manager; // Streamdeck connection manager, using mock class definition.
    StreamdeckContext fixture_context;

    // Create StreamdeckContext to test without any defined settings.
    static inline std::string fixture_context_id = "abc123";

    // Constants to be used for providing a valid DcsInterface.
    static inline std::string kDcsListenerPort = "1908"; // Port number to receive DCS updates from.
    static inline std::string kDcsSendPort = "1909";     // Port number which DCS commands will be sent to.
    static inline std::string kDcsIpAddress =
        "127.0.0.1"; // IP Address on which to communicate with DCS -- Default LocalHost.
};

TEST_F(StreamdeckContextTestFixture, update_context_state_when_no_dcs) {
    // Test -- With an unpopulated game state in dcs_interface, try to update context state.
    fixture_context.updateContextState(dcs_interface, &esd_connection_manager);
    // Expect no state or title change as default context state and title values have not changed.
    EXPECT_EQ(esd_connection_manager.context_, "");
}

TEST_F(StreamdeckContextTestFixture, update_context_state) {
    // Create StreamdeckContext initialized with settings to test.
    const std::string context_id = "def456";
    const json settings = {{"dcs_id_compare_monitor", "765"},
                           {"dcs_id_compare_condition", "EQUAL_TO"},
                           {"dcs_id_comparison_value", "2.0"},
                           {"dcs_id_string_monitor", "2026"}};
    StreamdeckContext fixture_context(context_id, settings);

    // Test -- With a populated game state in dcs_interface, try to update context state.
    // Send a single message from mock DCS that contains updates for multiple IDs.
    std::string mock_dcs_message = "header*761=1:765=2.00:2026=TEXT_STR:2027=4";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    fixture_context.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, context_id);
    EXPECT_EQ(esd_connection_manager.state_, 1);
    EXPECT_EQ(esd_connection_manager.title_, "TEXT_STR");
}

TEST_F(StreamdeckContextTestFixture, update_context_settings) {
    // Send a single message from mock DCS that contains updates for multiple IDs.
    std::string mock_dcs_message = "header*761=1:765=2.00:2026=TEXT_STR:2027=4";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();

    // Test 1 -- With no settings defined, streamdeck context should not send update.
    fixture_context.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");

    // Test 2 -- With populated settings, streamdeck context should try to update context state.
    json settings = {{"dcs_id_compare_monitor", "765"},
                     {"dcs_id_compare_condition", "EQUAL_TO"},
                     {"dcs_id_comparison_value", "2.0"},
                     {"dcs_id_string_monitor", "2026"}};
    fixture_context.updateContextSettings(settings);
    fixture_context.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, fixture_context_id);
    EXPECT_EQ(esd_connection_manager.state_, 1);
    EXPECT_EQ(esd_connection_manager.title_, "TEXT_STR");

    // Test 3 -- With cleared settings, streamdeck context should send default state and title.
    settings = {{"dcs_id_compare_monitor", ""},
                {"dcs_id_compare_condition", "EQUAL_TO"}, //< selection type always has some value.
                {"dcs_id_comparison_value", ""},
                {"dcs_id_string_monitor", ""}};
    fixture_context.updateContextSettings(settings);
    fixture_context.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, fixture_context_id);
    EXPECT_EQ(esd_connection_manager.state_, 0);
    EXPECT_EQ(esd_connection_manager.title_, "");
}

class StreamdeckContextComparisonTestFixture : public StreamdeckContextTestFixture {
  public:
    StreamdeckContextComparisonTestFixture()
        : // Create StreamdeckContexts with different comparison selections to test.
          context_with_equals("ctx_equals",
                              {{"dcs_id_compare_monitor", "123"},
                               {"dcs_id_compare_condition", "EQUAL_TO"},
                               {"dcs_id_comparison_value", std::to_string(comparison_value)}}),
          context_with_less_than("ctx_less_than",
                                 {{"dcs_id_compare_monitor", "123"},
                                  {"dcs_id_compare_condition", "LESS_THAN"},
                                  {"dcs_id_comparison_value", std::to_string(comparison_value)}}),
          context_with_greater_than("ctx_greater_than",
                                    {{"dcs_id_compare_monitor", "123"},
                                     {"dcs_id_compare_condition", "GREATER_THAN"},
                                     {"dcs_id_comparison_value", std::to_string(comparison_value)}}) {}

    const float comparison_value = 1.56F;
    StreamdeckContext context_with_equals;
    StreamdeckContext context_with_less_than;
    StreamdeckContext context_with_greater_than;
};

TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_zero) {
    // Test -- Compare 0 to non-zero comparison_value.
    const std::string mock_dcs_message = "header*123=0";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();

    esd_connection_manager.clear_buffer();
    context_with_equals.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
    esd_connection_manager.clear_buffer();
    context_with_less_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "ctx_less_than");
    EXPECT_EQ(esd_connection_manager.state_, 1);
    esd_connection_manager.clear_buffer();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
}

TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_lesser_value) {
    // Test -- Compare value less than comparison_value.
    const std::string mock_dcs_message = "header*123=" + std::to_string(comparison_value / 2.0F);
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();

    esd_connection_manager.clear_buffer();
    context_with_equals.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
    esd_connection_manager.clear_buffer();
    context_with_less_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "ctx_less_than");
    esd_connection_manager.clear_buffer();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
}

TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_equal_value) {
    // Test -- Compare value equal to comparison_value.
    const std::string mock_dcs_message = "header*123=" + std::to_string(comparison_value);
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();

    esd_connection_manager.clear_buffer();
    context_with_equals.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "ctx_equals");
    EXPECT_EQ(esd_connection_manager.state_, 1);
    esd_connection_manager.clear_buffer();
    context_with_less_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
    EXPECT_EQ(esd_connection_manager.state_, 0);
    esd_connection_manager.clear_buffer();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
}

TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_greater_value) {
    // Test -- Compare value greater than comparison_value.
    const std::string mock_dcs_message = "header*123=" + std::to_string(comparison_value * 2.0F);
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();

    esd_connection_manager.clear_buffer();
    context_with_equals.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
    EXPECT_EQ(esd_connection_manager.state_, 0);
    esd_connection_manager.clear_buffer();
    context_with_less_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");
    esd_connection_manager.clear_buffer();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "ctx_greater_than");
    EXPECT_EQ(esd_connection_manager.state_, 1);
}

// Test comparison to invalid values.

TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_integer) {
    // Send game state value as an integer -- should still resolve to float comparison.
    const std::string mock_dcs_message = "header*123=20";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 1);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_alphanumeric) {
    // Send game state value with letters -- should treat as string and not try comparison.
    const std::string mock_dcs_message = "header*123=20a";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 0);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_empty) {

    // Send game state as empty -- should treat as string and not try comparison.
    const std::string mock_dcs_message = "header*123=";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 0);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_dcs_id_float) {
    // Send DCS ID as a float -- should not read update.
    const std::string mock_dcs_message = "header*123.0=2.0";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 0);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_with_settings_id_as_float) {
    // Send valid game state, but make settings dcs_id monitor value invalid as a float.
    json settings;
    settings["dcs_id_compare_monitor"] = "123.0";
    context_with_greater_than.updateContextSettings(settings);
    const std::string mock_dcs_message = "header*123=2.0";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 0);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_with_invalid_settings_comparison_value) {
    // Send valid game state, but make settings comparison value invalid as a float.
    json settings;
    settings["dcs_id_compare_monitor"] = "123";
    settings["dcs_id_comparison_value"] = "1.0abc";
    context_with_greater_than.updateContextSettings(settings);
    const std::string mock_dcs_message = "header*123=2.0";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 0);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_float_with_leading_spaces) {
    // Use leading spaces, zeros, and integers -- should be compared as floats without problem.
    json settings;
    settings["dcs_id_compare_monitor"] = "  00123";
    settings["dcs_id_comparison_value"] = "  0001.00";
    context_with_greater_than.updateContextSettings(settings);
    const std::string mock_dcs_message = "header*123=   002";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 1);
}
TEST_F(StreamdeckContextComparisonTestFixture, dcs_id_float_compare_to_float_with_trailing_space) {
    // A trailing space is currently not handled.
    // TODO: Consider stripping trailing spaces.
    json settings;
    settings["dcs_id_compare_monitor"] = "123";
    settings["dcs_id_comparison_value"] = "1.0 "; //< Trailing space
    context_with_greater_than.updateContextSettings(settings);
    const std::string mock_dcs_message = "header*123=2.0";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();
    context_with_greater_than.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.state_, 0);
}

TEST_F(StreamdeckContextTestFixture, force_send_state_update) {
    // Test 1 -- With updateContextState and no detected state changes, no state is sent to connection manager.
    fixture_context.updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "");

    // Test -- force send will send current state regardless of state change.
    fixture_context.forceSendState(&esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "abc123");
    EXPECT_EQ(esd_connection_manager.state_, 0);
}

TEST_F(StreamdeckContextTestFixture, class_instances_within_container) {
    // This is more of a test of the MyStreamDeckPlugin.cpp use of this class than of the class itself.

    std::string mock_dcs_message = "header*1=a:2=b:3=c";
    mock_dcs.DcsSend(mock_dcs_message);
    dcs_interface.update_dcs_state();

    std::unordered_map<std::string, StreamdeckContext> ctx_map;
    ctx_map["ctx_a"] = StreamdeckContext("ctx_a", {{"dcs_id_string_monitor", "1"}});
    ctx_map["ctx_b"] = StreamdeckContext("ctx_b", {{"dcs_id_string_monitor", "2"}});
    ctx_map["ctx_c"] = StreamdeckContext("ctx_c", {{"dcs_id_string_monitor", "3"}});

    // Update state through map key.
    ctx_map["ctx_b"].updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "ctx_b");
    EXPECT_EQ(esd_connection_manager.title_, "b");

    // Update settings through map key.
    ctx_map["ctx_b"].updateContextSettings({{"dcs_id_string_monitor", "1"}});

    // Test that new settings are reflected in state send.
    ctx_map["ctx_b"].updateContextState(dcs_interface, &esd_connection_manager);
    EXPECT_EQ(esd_connection_manager.context_, "ctx_b");
    EXPECT_EQ(esd_connection_manager.title_, "a");
}
