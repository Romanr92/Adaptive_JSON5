# Licensing and redistribution

The repository uses [Apache License 2.0](../LICENSE) for its original contributions,
with the project notice `Copyright 2026 Razvan-Stefan Roman` in [NOTICE](../NOTICE),
including documentation, build configuration and original examples. The supplied
proposal is archived project material, not a license-change instruction: its MIT
suggestion does not replace the repository's existing Apache-2.0 license.
Third-party material keeps its own license and notices. Ownership is not assigned
by this document; contributors retain their applicable rights.

[NOTICE](../NOTICE) records incorporated/adapted material. [Dependencies and
provenance](dependencies.md) records versions and sources. The full license text,
including its appendix template, is preserved unchanged; the bracketed example
in that appendix is not an unresolved project copyright declaration.

## Distribution contents

For distributions of this repository or derivatives, include LICENSE, preserve
applicable attribution/copyright/patent/trademark notices, and retain NOTICE's
relevant attribution. Mark changes to upstream files prominently. These obligations
come from [Apache-2.0 section 4](https://www.apache.org/licenses/LICENSE-2.0).
This project is independent; no ASF, Google, AdaptivePi or vendor endorsement or
trademark permission is implied.

The installed development package includes LICENSE, NOTICE and the licensing and
provenance documents. It contains no GoogleTest library. GoogleTest's upstream
BSD-3-Clause notice is also included as a convenient record; this does not turn
GoogleTest into a runtime dependency.

If distributing the fetched GoogleTest source, retain its copyright, conditions
and disclaimer. If distributing a unit-test binary containing GoogleTest,
reproduce those in accompanying documentation/materials. The complete notice is
[LICENSES/GoogleTest-BSD-3-Clause.txt](../LICENSES/GoogleTest-BSD-3-Clause.txt), copied
from the pinned source. Its non-endorsement condition also applies. A local CMake
fetch is not a relicensing of GoogleTest.

Development programs such as CMake, Python, clang-format, Git and recommended
VS Code extensions are installed separately, not included in the library package.
Their exact installed artifacts retain their own licenses; copying or bundling
any of them requires checking that artifact's license and notices. A recommendation
or tool invocation is not an ownership or licensing grant for the tool.

Future external Unicode tables, syntax/schema corpora and schema/editor dependencies
are not presently imported. Record their exact source/version/license/notices before
incorporation; standards links and original examples do not grant redistribution
rights to upstream test suites. Add required texts and distribution rules when such
material is selected. Do not claim that all future dependencies are Apache-2.0.

This is a repository documentation/license review, not a legal opinion or a
certification of title, patent clearance, fitness or suitability for a particular
product. Actual releases must be reviewed against the material they distribute.
