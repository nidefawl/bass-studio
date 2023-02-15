#!/usr/bin/env bash
REPO=https://api.github.com/repos/nidefawl/daw
GITHUB_USER=nidefawl
# GITHUB_ADMIN_PAT=...
source ~/.ghsecrets

# Number of most recent versions to keep for each artifact:
KEEP=0

# A shortcut to call GitHub API.
ghapi() { 
    curl --silent --location --user $GITHUB_USER:$GITHUB_ADMIN_PAT "$@"; 
}

# A temporary file which receives HTTP response headers.
TMPFILE=/tmp/tmp.$$

# An associative array, key: artifact name, value: number of artifacts of that name.
declare -A ARTCOUNT

# Process all artifacts on this repository, loop on returned "pages".
URL=$REPO/actions/runs
echo "Processing artifacts from $URL"
if [[ -n "$URL" ]]; then

    # Get current page, get response headers in a temporary file.
    JSON=$(ghapi --dump-header $TMPFILE "$URL")
    
    # { "total_count": 2, "artifacts": [ ... ] }

    TOTAL=$(echo $JSON | jq '.total_count')
    
    WORKFLOW_RUNS_JSON_ARR=$(echo $JSON | jq '.workflow_runs')
    WORKFLOW_IDS=$(echo $WORKFLOW_RUNS_JSON_ARR | jq -r '.[].id')

    # iterate over workflow IDs
    for WORKFLOW_ID in $WORKFLOW_IDS; do
        echo "Processing workflow ID $WORKFLOW_ID"
        ARTIFACTS_URL=$REPO/actions/runs/$WORKFLOW_ID/artifacts
        if [[ -n "$ARTIFACTS_URL" ]]; then
            echo "Processing artifacts from $ARTIFACTS_URL"

            # Get current page, get response headers in a temporary file.
            JSON=$(ghapi --dump-header $TMPFILE "$ARTIFACTS_URL")
            TOTAL=$(echo $JSON | jq '.total_count')
            ARTIFACTS_JSON_ARR=$(echo $JSON | jq '.artifacts')
            ARTIFACT_IDS=$(echo $ARTIFACTS_JSON_ARR | jq -r '.[].id')

            # iterate over artifact IDs
            for ARTIFACT_ID in $ARTIFACT_IDS; do
                echo "Processing artifact ID $ARTIFACT_ID"
                ARTIFACT_URL=$REPO/actions/artifacts/$ARTIFACT_ID
                echo "Deleting artifact from $ARTIFACT_URL"
                ghapi -X DELETE "$ARTIFACT_URL"
            done
        fi

        # Delete the workflow run
        WORKFLOW_RUN_URL=$REPO/actions/runs/$WORKFLOW_ID
        echo "Deleting workflow run from $WORKFLOW_RUN_URL"
        ghapi -X DELETE "$WORKFLOW_RUN_URL"
    done 

fi