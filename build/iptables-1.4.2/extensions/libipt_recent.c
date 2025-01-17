/* Shared library add-on to iptables to add recent matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_recent.h>

/* Need these in order to not fail when compiling against an older kernel. */
#ifndef RECENT_NAME
#define RECENT_NAME	"ipt_recent"
#endif /* RECENT_NAME */

#ifndef RECENT_VER
#define RECENT_VER	"unknown"
#endif /* RECENT_VER */

#ifndef IPT_RECENT_NAME_LEN
#define IPT_RECENT_NAME_LEN	200
#endif /* IPT_RECENT_NAME_LEN */

static const struct option recent_opts[] = {
	{ .name = "set",      .has_arg = 0, .val = 201 }, 
	{ .name = "rcheck",   .has_arg = 0, .val = 202 }, 
	{ .name = "update",   .has_arg = 0, .val = 203 },
	{ .name = "seconds",  .has_arg = 1, .val = 204 }, 
	{ .name = "hitcount", .has_arg = 1, .val = 205 },
	{ .name = "remove",   .has_arg = 0, .val = 206 },
	{ .name = "rttl",     .has_arg = 0, .val = 207 },
	{ .name = "name",     .has_arg = 1, .val = 208 },
	{ .name = "rsource",  .has_arg = 0, .val = 209 },
	{ .name = "rdest",    .has_arg = 0, .val = 210 },
	{ .name = NULL }
};

static void recent_help(void)
{
	printf(
"recent match options:\n"
"[!] --set                       Add source address to list, always matches.\n"
"[!] --rcheck                    Match if source address in list.\n"
"[!] --update                    Match if source address in list, also update last-seen time.\n"
"[!] --remove                    Match if source address in list, also removes that address from list.\n"
"    --seconds seconds           For check and update commands above.\n"
"                                Specifies that the match will only occur if source address last seen within\n"
"                                the last 'seconds' seconds.\n"
"    --hitcount hits             For check and update commands above.\n"
"                                Specifies that the match will only occur if source address seen hits times.\n"
"                                May be used in conjunction with the seconds option.\n"
"    --rttl                      For check and update commands above.\n"
"                                Specifies that the match will only occur if the source address and the TTL\n"
"                                match between this packet and the one which was set.\n"
"                                Useful if you have problems with people spoofing their source address in order\n"
"                                to DoS you via this module.\n"
"    --name name                 Name of the recent list to be used.  DEFAULT used if none given.\n"
"    --rsource                   Match/Save the source address of each packet in the recent list table (default).\n"
"    --rdest                     Match/Save the destination address of each packet in the recent list table.\n"
RECENT_NAME " " RECENT_VER ": Stephen Frost <sfrost@snowman.net>.  http://snowman.net/projects/ipt_recent/\n");
}

static void recent_init(struct xt_entry_match *match)
{
	struct ipt_recent_info *info = (struct ipt_recent_info *)(match)->data;


	strncpy(info->name,"DEFAULT",IPT_RECENT_NAME_LEN);
	/* eventhough IPT_RECENT_NAME_LEN is currently defined as 200,
	 * better be safe, than sorry */
	info->name[IPT_RECENT_NAME_LEN-1] = '\0';
	info->side = IPT_RECENT_SOURCE;
}

#define RECENT_CMDS \
	(IPT_RECENT_SET | IPT_RECENT_CHECK | \
	IPT_RECENT_UPDATE | IPT_RECENT_REMOVE)

static int recent_parse(int c, char **argv, int invert, unsigned int *flags,
                        const void *entry, struct xt_entry_match **match)
{
	struct ipt_recent_info *info = (struct ipt_recent_info *)(*match)->data;
	switch (c) {
		case 201:
			if (*flags & RECENT_CMDS)
				exit_error(PARAMETER_PROBLEM,
					"recent: only one of `--set', `--rcheck' "
					"`--update' or `--remove' may be set");
			check_inverse(optarg, &invert, &optind, 0);
			info->check_set |= IPT_RECENT_SET;
			if (invert) info->invert = 1;
			*flags |= IPT_RECENT_SET;
			break;
			
		case 202:
			if (*flags & RECENT_CMDS)
				exit_error(PARAMETER_PROBLEM,
					"recent: only one of `--set', `--rcheck' "
					"`--update' or `--remove' may be set");
			check_inverse(optarg, &invert, &optind, 0);
			info->check_set |= IPT_RECENT_CHECK;
			if(invert) info->invert = 1;
			*flags |= IPT_RECENT_CHECK;
			break;

		case 203:
			if (*flags & RECENT_CMDS)
				exit_error(PARAMETER_PROBLEM,
					"recent: only one of `--set', `--rcheck' "
					"`--update' or `--remove' may be set");
			check_inverse(optarg, &invert, &optind, 0);
			info->check_set |= IPT_RECENT_UPDATE;
			if (invert) info->invert = 1;
			*flags |= IPT_RECENT_UPDATE;
			break;

		case 206:
			if (*flags & RECENT_CMDS)
				exit_error(PARAMETER_PROBLEM,
					"recent: only one of `--set', `--rcheck' "
					"`--update' or `--remove' may be set");
			check_inverse(optarg, &invert, &optind, 0);
			info->check_set |= IPT_RECENT_REMOVE;
			if (invert) info->invert = 1;
			*flags |= IPT_RECENT_REMOVE;
			break;

		case 204:
			info->seconds = atoi(optarg);
			break;

		case 205:
			info->hit_count = atoi(optarg);
			break;

		case 207:
			info->check_set |= IPT_RECENT_TTL;
			*flags |= IPT_RECENT_TTL;
			break;

		case 208:
			strncpy(info->name,optarg,IPT_RECENT_NAME_LEN);
			info->name[IPT_RECENT_NAME_LEN-1] = '\0';
			break;

		case 209:
			info->side = IPT_RECENT_SOURCE;
			break;

		case 210:
			info->side = IPT_RECENT_DEST;
			break;

		default:
			return 0;
	}

	return 1;
}

static void recent_check(unsigned int flags)
{
	if (!(flags & RECENT_CMDS))
		exit_error(PARAMETER_PROBLEM,
			"recent: you must specify one of `--set', `--rcheck' "
			"`--update' or `--remove'");
	if ((flags & IPT_RECENT_TTL) &&
	    (flags & (IPT_RECENT_SET | IPT_RECENT_REMOVE)))
		exit_error(PARAMETER_PROBLEM,
		           "recent: --rttl may only be used with --rcheck or "
		           "--update");
}

static void recent_print(const void *ip, const struct xt_entry_match *match,
                         int numeric)
{
	struct ipt_recent_info *info = (struct ipt_recent_info *)match->data;

	if (info->invert)
		fputc('!', stdout);

	printf("recent: ");
	if(info->check_set & IPT_RECENT_SET) printf("SET ");
	if(info->check_set & IPT_RECENT_CHECK) printf("CHECK ");
	if(info->check_set & IPT_RECENT_UPDATE) printf("UPDATE ");
	if(info->check_set & IPT_RECENT_REMOVE) printf("REMOVE ");
	if(info->seconds) printf("seconds: %d ",info->seconds);
	if(info->hit_count) printf("hit_count: %d ",info->hit_count);
	if(info->check_set & IPT_RECENT_TTL) printf("TTL-Match ");
	if(info->name) printf("name: %s ",info->name);
	if(info->side == IPT_RECENT_SOURCE) printf("side: source ");
	if(info->side == IPT_RECENT_DEST) printf("side: dest");
}

static void recent_save(const void *ip, const struct xt_entry_match *match)
{
	struct ipt_recent_info *info = (struct ipt_recent_info *)match->data;

	if (info->invert)
		printf("! ");

	if(info->check_set & IPT_RECENT_SET) printf("--set ");
	if(info->check_set & IPT_RECENT_CHECK) printf("--rcheck ");
	if(info->check_set & IPT_RECENT_UPDATE) printf("--update ");
	if(info->check_set & IPT_RECENT_REMOVE) printf("--remove ");
	if(info->seconds) printf("--seconds %d ",info->seconds);
	if(info->hit_count) printf("--hitcount %d ",info->hit_count);
	if(info->check_set & IPT_RECENT_TTL) printf("--rttl ");
	if(info->name) printf("--name %s ",info->name);
	if(info->side == IPT_RECENT_SOURCE) printf("--rsource ");
	if(info->side == IPT_RECENT_DEST) printf("--rdest ");
}

static struct xtables_match recent_mt_reg = {
    .name          = "recent",
    .version       = XTABLES_VERSION,
    .family        = PF_INET,
    .size          = XT_ALIGN(sizeof(struct ipt_recent_info)),
    .userspacesize = XT_ALIGN(sizeof(struct ipt_recent_info)),
    .help          = recent_help,
    .init          = recent_init,
    .parse         = recent_parse,
    .final_check   = recent_check,
    .print         = recent_print,
    .save          = recent_save,
    .extra_opts    = recent_opts,
};

void _init(void)
{
	xtables_register_match(&recent_mt_reg);
}
