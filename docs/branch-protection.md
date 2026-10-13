# Main branch protection

Repository settings must activate this policy; committing the JSON alone does not enforce it.

1. Download `.github/rulesets/main.json` from the pull request branch.
2. Open Settings > Rules > Rulesets > New ruleset > Import a ruleset.
3. Import the JSON, confirm enforcement is Active and the bypass list is empty, and save.
4. Confirm all four required check names match the successful checks on the pull request.

The rules require pull requests targeting `main`, current test results against the latest main, formatting, and GoogleTest on Linux, Windows and macOS with exceptions OFF and RTTI OFF. Direct pushes, force pushes and branch deletion are blocked, with no administrator bypass configured. No human review approval is required by this policy.

Merge the CI pull request after its checks pass. Existing optional exceptions-ON local presets remain available for compatibility; the required CI build uses OFF only.

See [GitHub ruleset documentation](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets).
