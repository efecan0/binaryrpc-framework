// This is the single entry point for the BinaryRPC library.
// Include this file to get access to the core public API.

#pragma once

// Core application and framework
#include "binaryrpc/core/app.hpp"
#include "binaryrpc/core/framework_api.hpp"

// Core data types
#include "binaryrpc/core/types.hpp"

// RPC and Session context
#include "binaryrpc/core/rpc/rpc_context.hpp"
#include "binaryrpc/core/session/session.hpp"

// Client identity for authentication
#include "binaryrpc/core/auth/ClientIdentity.hpp"

// Public interfaces for extension
#include "binaryrpc/core/interfaces/iplugin.hpp"
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/interfaces/IHandshakeInspector.hpp"
#include "binaryrpc/core/interfaces/IBackoffStrategy.hpp" 