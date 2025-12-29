#pragma once

#include <lib/log/svc_logger.hpp>

#include "lib.hpp"

/* Specify logger implementations here. */
inline exl::log::LoggerMgr<
    exl::log::SvcLogger
> Logging;