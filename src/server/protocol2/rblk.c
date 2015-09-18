#include "include.h"
#include "../../cmd.h"
#include "../../hexmap.h"
#include "../../protocol2/blk.h"

// For retrieving stored data.
struct rblk
{
	char *datpath;
	struct iobuf readbuf[DATA_FILE_SIG_MAX];
	unsigned int readbuflen;
};

#define RBLK_MAX	10

static int load_rblk(struct rblk *rblks, int ind, const char *datpath)
{
	int r;
	int ret=-1;
	int done=0;
	struct fzp *fzp;
	struct iobuf rbuf;

	free_w(&rblks[ind].datpath);
	if(!(rblks[ind].datpath=strdup_w(datpath, __func__)))
		goto end;

	logp("swap %d to: %s\n", ind, datpath);

	if(!(fzp=fzp_open(datpath, "rb")))
		goto end;
	for(r=0; r<DATA_FILE_SIG_MAX; r++)
	{
		switch(iobuf_fill_from_fzp_data(&rbuf, fzp))
		{
			case 0: if(rbuf.cmd!=CMD_DATA)
				{
					logp("unknown cmd in %s: %c\n",
						__func__, rbuf.cmd);
					goto end;
				}
				iobuf_move(&rblks[ind].readbuf[r], &rbuf);
				continue;
			case 1: done++;
				break;
			default: goto end;
		}
		if(done) break;
	}
	rblks[ind].readbuflen=r;
	ret=0;
end:
	fzp_close(&fzp);
	return ret;
}

static struct rblk *get_rblk(struct rblk *rblks, const char *datpath)
{
	static int current_ind=0;
	static int last_swap_ind=0;
	int ind=current_ind;

	while(1)
	{
		if(!rblks[ind].datpath)
		{
			if(load_rblk(rblks, ind, datpath)) return NULL;
			last_swap_ind=ind;
			current_ind=ind;
			return &rblks[current_ind];
		}
		else if(!strcmp(rblks[ind].datpath, datpath))
		{
			current_ind=ind;
			return &rblks[current_ind];
		}
		ind++;
		if(ind==RBLK_MAX) ind=0;
		if(ind==current_ind)
		{
			// Went through all RBLK_MAX entries.
			// Replace the oldest one.
			ind=last_swap_ind+1;
			if(ind==RBLK_MAX) ind=0;
			if(load_rblk(rblks, ind, datpath)) return NULL;
			last_swap_ind=ind;
			current_ind=ind;
			return &rblks[current_ind];
		}
	}
}

int rblk_retrieve_data(const char *datpath, struct blk *blk)
{
	static char fulldatpath[256]="";
	static struct rblk *rblks=NULL;
	char *cp;
	unsigned int datno;
	struct rblk *rblk;

	snprintf(fulldatpath, sizeof(fulldatpath), "%s/%s",
		datpath, uint64_to_savepathstr_with_sig(blk->savepath));

//printf("x: %s\n", fulldatpath);
	if(!(cp=strrchr(fulldatpath, '/')))
	{
		logp("Could not parse data path: %s\n", fulldatpath);
		return -1;
	}
	*cp=0;
	cp++;
	datno=strtoul(cp, NULL, 16);
//printf("y: %s\n", fulldatpath);

	if(!rblks
	  && !(rblks=(struct rblk *)
		calloc_w(RBLK_MAX, sizeof(struct rblk), __func__)))
			return -1;

	if(!(rblk=get_rblk(rblks, fulldatpath)))
	{
		return -1;
	}

//	printf("lookup: %s (%s)\n", fulldatpath, cp);
	if(datno>rblk->readbuflen)
	{
		logp("dat index %d is greater than readbuflen: %d\n",
			datno, rblk->readbuflen);
		return -1;
	}
	blk->data=rblk->readbuf[datno].buf;
	blk->length=rblk->readbuf[datno].len;
//	printf("length: %d\n", blk->length);

        return 0;
}