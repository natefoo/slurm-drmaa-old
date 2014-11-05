/*****************************************************************************\
 *  mult_cluster.c - definitions for sbatch to submit job to multiple clusters
 *****************************************************************************
 *  Copyright (C) 2010 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Danny Auble <da@llnl.gov>,
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <drmaa_utils/common.h>
#include <string.h>

#include "mult_cluster.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/parse_time.h"
#include "src/common/read_config.h"

#include "src/common/slurm_protocol_api.h"
#include <slurm_drmaa/slurm_missing.h>

typedef struct {
	slurmdb_cluster_rec_t *cluster_rec;
	int preempt_cnt;
	time_t start_time;
} local_cluster_rec_t;

static char *local_cluster_name; /* name of local_cluster      */

void _destroy_local_cluster_rec(void *object)
{
	xfree(object);
}

static int _sort_local_cluster(void *v1, void *v2)
{
	local_cluster_rec_t* rec_a = *(local_cluster_rec_t**)v1;
	local_cluster_rec_t* rec_b = *(local_cluster_rec_t**)v2;

	if (rec_a->start_time < rec_b->start_time)
		return -1;
	else if (rec_a->start_time > rec_b->start_time)
		return 1;

	if (rec_a->preempt_cnt < rec_b->preempt_cnt)
		return -1;
	else if (rec_a->preempt_cnt > rec_b->preempt_cnt)
		return 1;

	if (!strcmp(local_cluster_name, rec_a->cluster_rec->name))
		return -1;
	else if (!strcmp(local_cluster_name, rec_b->cluster_rec->name))
		return 1;

	return 0;
}

/*
 * We don't use the api here because it does things we aren't needing
 * like printing out information and not returning times.
 */
local_cluster_rec_t *_job_will_run (job_desc_msg_t *req)
{
	slurm_msg_t req_msg, resp_msg;
	will_run_response_msg_t *will_run_resp;
	int rc;
	char buf[64];
	char *type = "processors";
	local_cluster_rec_t *local_cluster = NULL;
	/* req.immediate = true;    implicit */

	slurm_msg_t_init(&req_msg);
	req_msg.msg_type = REQUEST_JOB_WILL_RUN;
	req_msg.data     = req;

	rc = slurm_send_recv_controller_msg(&req_msg, &resp_msg);

	if (rc < 0) {
		slurm_seterrno(SLURM_SOCKET_ERROR);
		return NULL;
	}
	switch (resp_msg.msg_type) {
	case RESPONSE_SLURM_RC:
		rc = ((return_code_msg_t *) resp_msg.data)->return_code;
		slurm_free_return_code_msg(resp_msg.data);
		if (rc)
			slurm_seterrno(rc);
		break;
	case RESPONSE_JOB_WILL_RUN:
		if (working_cluster_rec->flags & CLUSTER_FLAG_BG)
			type = "cnodes";
		will_run_resp = (will_run_response_msg_t *) resp_msg.data;
		slurm_make_time_str(&will_run_resp->start_time,
				    buf, sizeof(buf));
		fsd_log_debug(("Job %u to start at %s on cluster %s using %u %s on %s",
		      will_run_resp->job_id, buf, working_cluster_rec->name,
		      will_run_resp->proc_cnt, type,
		      will_run_resp->node_list));
		local_cluster = xmalloc(sizeof(local_cluster_rec_t));
		local_cluster->cluster_rec = working_cluster_rec;
		local_cluster->start_time = will_run_resp->start_time;
		if (will_run_resp->preemptee_job_id) {
			local_cluster->preempt_cnt =
				slurm_list_count(will_run_resp->preemptee_job_id);
		}

		slurm_free_will_run_response_msg(will_run_resp);
		break;
	default:
		slurm_seterrno(SLURM_UNEXPECTED_MSG_ERROR);
		return NULL;
		break;
	}
	return local_cluster;
}

int slurmdrmaa_set_first_avail_cluster(const char * clusters, job_desc_msg_t *req)
{
	int rc = SLURM_SUCCESS;
	ListIterator itr;
	local_cluster_rec_t *local_cluster = NULL;
	char buf[64];
	bool host_set = false;
	List ret_list = NULL;
	List cluster_list = NULL;

	fsd_log_enter(( "({clusters=%s})", clusters));

	cluster_list = slurmdb_get_info_cluster((char *)clusters);

	/* return if we only have 1 or less clusters here */
	if (!cluster_list || !slurm_list_count(cluster_list)) {
		return rc;
	} else if (slurm_list_count(cluster_list) == 1) {
		working_cluster_rec = slurm_list_peek(cluster_list);
		return rc;
	}

	if ((req->alloc_node == NULL) &&
	    (gethostname_short(buf, sizeof(buf)) == 0)) {
		req->alloc_node = buf;
		host_set = true;
	}

	ret_list = slurm_list_create(_destroy_local_cluster_rec);
	itr = slurm_list_iterator_create(cluster_list);
	while ((working_cluster_rec = slurm_list_next(itr))) {
		if ((local_cluster = _job_will_run(req)))
			slurm_list_append(ret_list, local_cluster);
		else
			fsd_log_debug(("Problem with submit to cluster %s: %m",
			      working_cluster_rec->name));
	}
	slurm_list_iterator_destroy(itr);

	if (host_set)
		req->alloc_node = NULL;

	if (!slurm_list_count(ret_list)) {
		error("Can't run on any of the clusters given");
		rc = SLURM_ERROR;
		goto end_it;
	}

	/* sort the list so the first spot is on top */
	local_cluster_name = slurm_get_cluster_name();
	slurm_list_sort(ret_list, (ListCmpF)_sort_local_cluster);
	xfree(local_cluster_name);
	local_cluster = slurm_list_peek(ret_list);

	/* set up the working cluster and be done */
	itr = slurm_list_iterator_create(cluster_list);
	while ((working_cluster_rec = slurm_list_next(itr))) {
		if (working_cluster_rec == local_cluster->cluster_rec)
		{
			slurm_list_remove(itr);
			break;
		}
	}
	slurm_list_iterator_destroy(itr);

	fsd_log_debug(("set working_cluster_rec = %s",working_cluster_rec->name));
	fsd_log_return(( "" ));
end_it:
	slurm_list_destroy(ret_list);
	slurm_list_destroy(cluster_list);

	return rc;
}

/* from slurm src/common/read_config.c */
int
gethostname_short (char *name, size_t len)
{
	int error_code, name_len;
	char *dot_ptr, path_name[1024];

	error_code = gethostname (path_name, sizeof(path_name));
	if (error_code)
		return error_code;

	dot_ptr = strchr (path_name, '.');
	if (dot_ptr == NULL)
		dot_ptr = path_name + strlen(path_name);
	else
		dot_ptr[0] = '\0';

	name_len = (dot_ptr - path_name);
	if (name_len > len)
		return ENAMETOOLONG;

	strcpy (name, path_name);
	return 0;
}
