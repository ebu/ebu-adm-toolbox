"""
adapted from the sphinx recipe example, license below

Unless otherwise indicated, all code in the Sphinx project is licenced under the
two clause BSD licence below.

Copyright (c) 2007-2022 by the Sphinx team (see AUTHORS file).
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

from collections import defaultdict
from docutils.parsers.rst import directives

from sphinx import addnodes
from sphinx.directives import ObjectDescription
from sphinx.domains import Domain, Index
from sphinx.roles import XRefRole
from sphinx.util.nodes import make_refnode
from sphinx.util.docfields import TypedField


class ProcessDirective(ObjectDescription):
    """A custom directive that describes a process."""

    has_content = True
    required_arguments = 1
    option_spec = {
        "contains": directives.unchanged_required,
    }

    doc_field_types = [
        TypedField("parameter", label="Parameters", names=("param", "parameter")),
        TypedField("input", label="Input Ports", names=("input",)),
        TypedField("output", label="Output Ports", names=("output",)),
    ]

    def handle_signature(self, sig, signode):
        signode += addnodes.desc_name(text=sig)
        return sig

    def add_target_and_index(self, name_cls, sig, signode):
        node_id = f"process-{sig}"
        signode["ids"].append(node_id)
        processes = self.env.get_domain("process")
        processes.add_process(sig)

        self.indexnode["entries"].append(
            ("single", f"{sig} (process type)", node_id, "", None)
        )


class ProcessIndex(Index):
    """A custom index that creates an process matrix."""

    name = "process"
    localname = "Process Index"
    shortname = "Process"

    def generate(self, docnames=None):
        content = defaultdict(list)

        # sort the list of processes in alphabetical order
        processes = self.domain.get_objects()
        processes = sorted(processes, key=lambda process: process[0])

        # generate the expected output, shown below, from the above using the
        # first letter of the process as a key to group thing
        #
        # name, subtype, docname, anchor, extra, qualifier, description
        for _name, dispname, typ, docname, anchor, _priority in processes:
            content[dispname[0].lower()].append(
                (dispname, 0, docname, anchor, docname, "", typ)
            )

        # convert the dict to the sorted list of tuples expected
        content = sorted(content.items())

        return content, True


class ProcessDomain(Domain):

    name = "process"
    label = "Process"
    roles = {"ref": XRefRole()}
    directives = {
        "process": ProcessDirective,
    }
    indices = {
        ProcessIndex,
    }
    initial_data = {
        "processes": [],  # object list
    }

    def get_full_qualified_name(self, node):
        return "{}.{}".format("process", node.arguments[0])

    def get_objects(self):
        for obj in self.data["processes"]:
            yield obj

    def resolve_xref(self, env, fromdocname, builder, typ, target, node, contnode):
        matches = [
            (docname, anchor)
            for name, sig, typ, docname, anchor, prio in self.get_objects()
            if sig == target
        ]

        if len(matches) > 0:
            todocname = matches[0][0]
            targ = matches[0][1]

            return make_refnode(builder, fromdocname, todocname, targ, contnode, targ)
        else:
            return None

    def add_process(self, signature):
        """Add a new process to the domain."""
        name = f"process.{signature}"
        anchor = f"process-{signature}"

        # name, dispname, type, docname, anchor, priority
        self.data["processes"].append(
            (name, signature, "Process", self.env.docname, anchor, 0)
        )


def setup(app):
    app.add_domain(ProcessDomain)

    return {
        "version": "0.1",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
