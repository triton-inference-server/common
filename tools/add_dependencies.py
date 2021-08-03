#!/usr/bin/env python3
# Copyright 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import subprocess
import sys
import re
import docker


def add_dependencies(image, dependencies):
    # Helper function for all installations. For the list of dependencies provided,
    # apt-get install dependency if it does not exist in docker image
    df = ""
    need_to_install_dependencies = False
    print("pulling container:{}".format(image))
    p = subprocess.run(['docker', 'pull', image])
    if p.returncode != 0:
        print('ERROR: docker pull container {} failed, {}'.format(
            image, p.stderr))
        sys.exit(1)

    df += '''
RUN apt-get update && \
    apt-get install -y --no-install-recommends \ 
'''
    client = docker.from_env()
    for dep in dependencies:
        searchString = "dpkg -s {}".format(dep)
        try:
            var = client.containers.run(image, searchString)
            print("found package: {}".format(dep))
        # dpkg-query returns 1 when cannot find package with error message
        except docker.errors.ContainerError as var:
            packages = re.search(
                "package '{}' is not installed and no information is available".
                format(dep), str(var))
            if packages == None:
                print("Unexpected throw on docker run:" + str(var))
                sys.exit(1)
            else:
                need_to_install_dependencies = True
                df += '''{} \ 
'''.format(dep)
    df += '''&& rm -rf /var/lib/apt/lists/*
'''
    return df if need_to_install_dependencies else ""
