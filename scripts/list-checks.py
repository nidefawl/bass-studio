# lists clang-tidy checks state. run in repo root

with open('clang-tidy-checks.yml') as f:
    all_checks = f.read().split('\n')
    # remove empty lines
    all_checks = [x for x in all_checks if x]

with open('checks-enabled.yml') as f:
    enabled_checks = f.read().split('\n')
    # remove empty lines
    enabled_checks = [x for x in enabled_checks if x]

# remove all enabled checks from all_checks
disabled_checks = [x for x in all_checks if x not in enabled_checks]
# print number of available, disabled and enabled checks
print('Available checks: {}'.format(len(all_checks)))
print('Enabled checks: {}'.format(len(enabled_checks)))
print('Disabled checks: {}'.format(len(disabled_checks)))
with open('disabled-checks.yml', 'w') as f:
    for check in disabled_checks:
        f.write(f'    -{check},\n')
