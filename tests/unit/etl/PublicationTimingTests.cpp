#include "util/PublicationTiming.hpp"
#include "util/log/Logger.hpp"

#include <gtest/gtest.h>

#include <optional>

TEST(PublicationTimingTests, DisabledTimerDoesNotRecord)
{
    util::Logger log{"ETL"};
    util::PublicationTiming timer{log, 1, "test", std::nullopt};
    EXPECT_FALSE(timer.enabled());
    timer.mark("disabled");
    EXPECT_FALSE(timer.enabled());
}

TEST(PublicationTimingTests, ExplicitStartEnablesStageRecording)
{
    util::Logger log{"ETL"};
    util::PublicationTiming timer{log, 1, "test", util::PublicationTiming::Clock::now()};
    EXPECT_TRUE(timer.enabled());
    timer.mark("first");
    timer.mark("second");
}
