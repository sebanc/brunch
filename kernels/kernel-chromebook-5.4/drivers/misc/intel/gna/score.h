/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2017-2021 Intel Corporation */

#ifndef __GNA_SCORE_H__
#define __GNA_SCORE_H__

struct gna_file_private;
struct gna_compute_cfg;
struct gna_private;
struct gna_request;

int gna_validate_score_config(struct gna_compute_cfg *compute_cfg,
			struct gna_file_private *file_priv);

int gna_score(struct gna_request *score_request);

#endif // __GNA_SCORE_H__
