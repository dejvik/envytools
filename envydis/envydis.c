/*
 *
 * Copyright (C) 2009 Marcin Kościelnicki <koriakin@0x04.net>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "dis.h"
#include <libgen.h>

/*
 * Options:
 *
 *  -b <base>	Use a fake base address
 *  -s <num>	Skip this many initial bytes of the input
 *  -l <num>	Limit disassembling to <num> bytes of input
 *  -w		Treat input as a sequence of 32-bit words instead of bytes
 *  -n		Disable color escape sequences in output
 */

int main(int argc, char **argv) {
	FILE *infile = stdin;
	struct disisa *isa = 0;
	struct label *labels = 0;
	int labelsnum = 0;
	int labelsmax = 0;
	int w = 0, bin = 0, quiet = 0;
	const char *varname = 0;
	argv[0] = basename(argv[0]);
	if (!strcmp(argv[0], "nv50dis")) {
		isa = nv50_isa;
		w = 1;
	}
	if (!strcmp(argv[0], "nvc0dis")) {
		isa = nvc0_isa;
		w = 1;
	}
	if (!strcmp(argv[0], "ctxdis")) {
		isa = ctx_isa;
		w = 1;
	}
	if (!strcmp(argv[0], "fucdis") || !strcmp(argv[0], "fµcdis"))
		isa = fuc_isa;
	if (!strcmp(argv[0], "pmsdis"))
		isa = pms_isa;
	if (!strcmp(argv[0], "vp2dis"))
		isa = vp2_isa;
	if (!strcmp(argv[0], "vp3mdis"))
		isa = vp3m_isa;
	if (!strcmp(argv[0], "macrodis")) {
		isa = macro_isa;
		w = 1;
	}
	int ptype = -1;
	int vartype = -1;
	int c;
	unsigned base = 0, skip = 0, limit = 0;
	while ((c = getopt (argc, argv, "vgfpcsb:d:l:m:V:wWinqu:M:")) != -1)
		switch (c) {
			case 'v':
				ptype = VP;
				break;
			case 'g':
				ptype = GP;
				break;
			case 'f':
			case 'p':
				ptype = FP;
				break;
			case 'c':
				ptype = CP;
				break;
			case 's':
				ptype = VP|GP|FP;
				break;
			case 'b':
				sscanf(optarg, "%x", &base);
				break;
			case 'd':
				sscanf(optarg, "%x", &skip);
				break;
			case 'l':
				sscanf(optarg, "%x", &limit);
				break;
			case 'w':
				w = 1;
				break;
			case 'W':
				w = 2;
				break;
			case 'i':
				bin = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'n':
				cnorm = "";
				cname = "";
				creg0 = "";
				creg1 = "";
				cmem = "";
				cnum = "";
				cunk = "";
				cbtarg = "";
				cctarg = "";
				cbctarg = "";
				break;
			case 'm':
				if (!strcmp(optarg, "nv50"))
					isa = nv50_isa;
				else if (!strcmp(optarg, "nvc0"))
					isa = nvc0_isa;
				else if (!strcmp(optarg, "ctx"))
					isa = ctx_isa;
				else if (!strcmp(optarg, "fuc") || !strcmp(optarg, "fµc"))
					isa = fuc_isa;
				else if (!strcmp(optarg, "pms"))
					isa = pms_isa;
				else if (!strcmp(optarg, "vp2"))
					isa = vp2_isa;
				else if (!strcmp(optarg, "vp3m"))
					isa = vp3m_isa;
				else if (!strcmp(optarg, "macro"))
					isa = macro_isa;
				else {
					fprintf (stderr, "Unknown architecure \"%s\"!\n", optarg);
					return 1;
				}
				break;
			case 'V':
				varname = optarg;
				break;
			case 'M':
				{
					FILE *mapfile = fopen(optarg, "r");
					if (!mapfile) {
						perror(optarg);
						return 1;
					}
					struct label nl;
					char type;
					char buf[1000] = "";
					while (fscanf(mapfile, " %c%llx%s", &type, &nl.val, buf) >= 2) {
						switch (type) {
							case 'B':
								nl.type = 1;
								break;
							case 'C':
								nl.type = 2;
								break;
							case 'E':
								nl.type = 4;
								break;
							case 'S':
								nl.type = 0x20;
								break;
							default:
								fprintf (stderr, "Unknown label type %c\n", type);
								return 1;
						}
						if (*buf)
							nl.name = strdup(buf);
						else
							nl.name = 0;
						RNN_ADDARRAY(labels, nl);
					}
					break;
				}
			case 'u':
				{
					struct label nl;
					sscanf(optarg, "%llx", &nl.val);
					nl.type = 1;
					nl.name = 0;
					RNN_ADDARRAY(labels, nl);
					break;
				}
		}
	if (optind < argc) {
		if (!(infile = fopen(argv[optind], "r"))) {
			perror(argv[optind]);
			return 1;
		}
		optind++;
		if (optind < argc) {
			fprintf (stderr, "Too many parameters!\n");
			return 1;
		}
	}
	if (!isa) {
		fprintf (stderr, "No architecture specified!\n");
		return 1;
	}
	if (varname) {
		if (isa == nv50_isa) {
			if (!strcmp(varname, "nv50"))
				vartype = NV50;
			else if (!strcmp(varname, "nv84"))
				vartype = NV84;
			else if (!strcmp(varname, "nva0"))
				vartype = NVA0;
			else if (!strcmp(varname, "nvaa"))
				vartype = NVAA;
			else if (!strcmp(varname, "nva3"))
				vartype = NVA3;
			else {
				fprintf (stderr, "Unknown variant \"%s\"!\n", varname);
				return 1;
			}
		} else if (isa == ctx_isa) {
			if (!strcmp(varname, "nv40"))
				vartype = CTX_NV40;
			else if (!strcmp(varname, "nv50"))
				vartype = CTX_NV50;
			else {
				fprintf (stderr, "Unknown variant \"%s\"!\n", varname);
				return 1;
			}
		} else if (isa == fuc_isa) {
			if (!strcmp(varname, "nv98"))
				vartype = FUC_NV98;
			else if (!strcmp(varname, "nva3"))
				vartype = FUC_NVA3;
			else {
				fprintf (stderr, "Unknown variant \"%s\"!\n", varname);
				return 1;
			}
		} else {
			fprintf (stderr, "Unknown variant \"%s\"!\n", varname);
			return 1;
		}
	}
	int num = 0;
	int maxnum = 16;
	uint8_t *code = malloc (maxnum);
	ull t;
	if (bin) {
		int c;
		while ((c = getc(infile)) != EOF) {
			if (num + 3 >= maxnum) maxnum *= 2, code = realloc (code, maxnum);
			code[num++] = c;
		}
	} else {
		while (!feof(infile) && fscanf (infile, "%llx", &t) == 1) {
			if (num + 3 >= maxnum) maxnum *= 2, code = realloc (code, maxnum);
			if (w == 2) {
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
			} else if (w) {
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
				t >>= 8;
				code[num++] = t & 0xff;
			} else
				code[num++] = t;
			fscanf (infile, " ,");
		}
	}
	if (num <= skip)
		return 0;
	int cnt = num - skip;
	if (limit && limit < cnt)
		cnt = limit;
	envydis (isa, stdout, code+skip, base, cnt, vartype, ptype, quiet, labels, labelsnum);
	return 0;
}
