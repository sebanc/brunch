// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#include "intel_pxp_context.h"
#include <linux/random.h>

/**
 * intel_pxp_ctx_init - To init a pxp context.
 * @ctx: pointer to ctx structure.
 */
void intel_pxp_ctx_init(struct pxp_context *ctx)
{
	get_random_bytes(&ctx->id, sizeof(ctx->id));

	ctx->global_state_attacked = false;
	ctx->arb_pxp_tag = 0;

	mutex_init(&ctx->mutex);

	INIT_LIST_HEAD(&ctx->type0_sessions);
	INIT_LIST_HEAD(&ctx->type1_sessions);
}

/**
 * intel_pxp_ctx_fini - To finish the pxp context.
 * @ctx: pointer to ctx structure.
 */
void intel_pxp_ctx_fini(struct pxp_context *ctx)
{
	ctx->id = 0;
}
