#include "ActivationToken.h"
#include "Server.h"

ActivationToken::ActivationToken(Server *server,
                                 wlr_xdg_activation_token_v1 *token)
    : token(token) {
    activated = false;
    had_focus = token->surface != nullptr;
    token->data = this;

    wl_list_init(&link);
    wl_list_insert(&server->pending_activation_tokens, &link);
}

ActivationToken::~ActivationToken() {
    if (!wl_list_empty(&link))
        wl_list_remove(&link);

    if (token)
        token->data = nullptr;
}

void ActivationToken::consume() {
    if (token) {
        if (!activated)
            token->data = nullptr;
        token = nullptr;
    }

    if (!wl_list_empty(&link))
        wl_list_remove(&link);
    wl_list_init(&link);
}
