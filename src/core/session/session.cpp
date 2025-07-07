#include "binaryrpc/core/session/session.hpp"

namespace binaryrpc {

    Session::Session(ClientIdentity ident, std::string legacyId)
        : ident_(std::move(ident)), legacyId_(std::move(legacyId)), legacyConn_(nullptr)
    { }

}