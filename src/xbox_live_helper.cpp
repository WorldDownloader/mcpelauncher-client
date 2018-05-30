#include <msa/client/compact_token.h>
#include <minecraft/Xbox.h>
#include <log.h>
#include "xbox_live_helper.h"

using namespace simpleipc;

std::string const XboxLiveHelper::MSA_CLIENT_ID = "android-app://com.mojang.minecraftpe.H62DKCBHJP6WXXIV7RBFOGOL4NAK4E6Y";
std::string const XboxLiveHelper::MSA_COBRAND_ID = "90023";

void XboxLiveHelper::invokeMsaAuthFlow(
        std::function<void(std::string const& cid, std::string const& binaryToken)> success_cb,
        std::function<void(simpleipc::rpc_error_code, std::string const&)> error_cb) {
    client.pickAccount(MSA_CLIENT_ID, MSA_COBRAND_ID).call([this, success_cb, error_cb](rpc_result<std::string> res) {
        if (!res.success()) {
            error_cb(res.error_code(), res.error_text());
            return;
        }

        std::string cid = res.data();
        requestXblToken(cid, false, success_cb, error_cb);
    });
}

xbox::services::xbox_live_result<xbox::services::system::token_and_signature_result> XboxLiveHelper::invokeXblLogin(
        xbox::services::system::user_auth_android* auth, std::string const& cid, std::string const& binaryToken) {
    using namespace xbox::services::system;
    auto auth_mgr = xbox::services::system::auth_manager::get_auth_manager_instance();
    auth_mgr->set_rps_ticket(binaryToken);
    auto initTask = auth_mgr->initialize_default_nsal();
    auto initRet = initTask.get();
    if (initRet.code != 0)
        throw std::runtime_error("Failed to initialize default nsal");
    std::vector<token_identity_type> types = {(token_identity_type) 3, (token_identity_type) 1,
                                              (token_identity_type) 2};
    auto config = auth_mgr->get_auth_config();
    config->set_xtoken_composition(types);
    std::string const& endpoint = config->xbox_live_endpoint().std();
    Log::trace("XboxLiveHelper", "Xbox Live Endpoint: %s", endpoint.c_str());
    auto task = auth_mgr->internal_get_token_and_signature("GET", endpoint, endpoint, std::string(), std::vector<unsigned char>(), false, false, std::string()); // I'm unsure about the vector (and pretty much only about the vector)
    Log::trace("XboxLiveHelper", "Get token and signature task started!");
    auto ret = task.get();
    Log::debug("XboxLiveHelper", "User info received! Status: %i", ret.code);
    Log::debug("XboxLiveHelper", "Gamertag = %s, age group = %s, web account id = %s\n", ret.data.gamertag.c_str(), ret.data.age_group.c_str(), ret.data.web_account_id.c_str());
    return ret;
}

simpleipc::client::rpc_call<std::shared_ptr<msa::client::Token>> XboxLiveHelper::requestXblToken(std::string const& cid,
                                                                                                 bool silent) {
    return client.requestToken(cid, {"user.auth.xboxlive.com", "mbi_ssl"}, MSA_CLIENT_ID, silent);
}

void XboxLiveHelper::requestXblToken
        (std::string const& cid, bool silent,
         std::function<void(std::string const& cid, std::string const& binaryToken)> success_cb,
         std::function<void(simpleipc::rpc_error_code, std::string const&)> error_cb) {
    requestXblToken(cid, silent).call([cid, success_cb, error_cb](rpc_result<std::shared_ptr<msa::client::Token>> res) {
        if (res.success() && res.data() && res.data()->getType() == msa::client::TokenType::Compact) {
            auto token = msa::client::token_pointer_cast<msa::client::CompactToken>(res.data());
            success_cb(cid, token->getBinaryToken());
        } else {
            if (res.success())
                error_cb(simpleipc::rpc_error_codes::internal_error, "Invalid token received from the MSA daemon");
            else
                error_cb(res.error_code(), res.error_text());
        }
    });
}