#!/bin/bash

TOOL_DIR="../build"

clear

echo "Decompiling models..."

MDL_TOTAL=0
MDL_SUCCESS=0

for MDL_NAME in *.mdl; do
    MDL_BASE_NAME=${MDL_NAME%.*}
    
    echo ""
    echo "=== $MDL_NAME ==="
    echo ""
    
    ((MDL_TOTAL+=1))
    
    "$TOOL_DIR/decompmdl" "$MDL_NAME"
    
    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        continue
    fi
    
    ((MDL_SUCCESS+=1))
done

echo ""
echo "================"
echo ""

echo "Decompiled $MDL_SUCCESS / $MDL_TOTAL models(s)."
