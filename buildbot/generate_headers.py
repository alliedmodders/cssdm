#!/usr/bin/env python3
# vim: set ts=8 sts=2 sw=2 tw=99 et:
import re
import os, sys
import subprocess

argv = sys.argv[1:]
if len(argv) < 2:
    sys.stderr.write('Usage: generate_headers.py <source_path> <output_folder>\n')
    sys.exit(1)

source_dir = os.path.abspath(os.path.normpath(argv[0]))
build_dir = os.path.normpath(argv[1])

def GetVersion():
	with open(os.path.join(source_dir, '.git', 'HEAD')) as fp:
		head_contents = fp.read().strip()
		if re.search(r'^[a-fA-F0-9]{40}$', head_contents):
			git_head_path = os.path.join(source_dir, '.git', 'HEAD')
		else:
			git_state = head_contents.split(':')[1].strip()
			git_head_path = os.path.join(source_dir, '.git', git_state)
			if not os.path.exists(git_head_path):
				git_head_path = os.path.join(source_dir, '.git', 'HEAD')

	return open(git_head_path, 'r').read().strip();
	
def PerformReversioning():
	rev = GetVersion()
	
	args = ['git', 'rev-list', '--count', 'HEAD']
	cset = subprocess.run(args, capture_output=True, text=True).stdout.strip()
	
	with open(os.path.join(source_dir, 'product.version'), 'r') as productFile:
		productContents = productFile.read()
	m = re.match(r'(\d+)\.(\d+)\.(\d+)(.*)', productContents)
	if m == None:
		raise Exception('Could not detremine product version')
	major, minor, release, tag = m.groups()

	if not os.path.isdir(build_dir):
		os.makedirs(build_dir)
	incFile = open(os.path.join(build_dir, 'cssdm_version_auto.h'), 'w')
	incFile.write("""
#ifndef _CSSDM_AUTO_VERSION_INFORMATION_H_
#define _CSSDM_AUTO_VERSION_INFORMATION_H_

#define CSSDM_BUILD_STRING 		\"{0}\"
#define CSSDM_BUILD_UNIQUEID		\"{1}:{2}\" CSSDM_BUILD_STRING
#define CSSDM_FULL_VERSION		\"{3}.{4}.{5}\" CSSDM_BUILD_STRING
#define CSSDM_FILE_VERSION		{6},{7},{8},0

#endif /* _CSSDM_AUTO_VERSION_INFORMATION_H_ */

""".format(tag, rev, cset, major, minor, release, major, minor, release))
	incFile.close()

PerformReversioning()
