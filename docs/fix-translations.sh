#!/bin/bash

# Fix Korean translations in generated HTML files
echo "Fixing Korean translations in HTML files..."

if [ -d "html" ]; then
    cd html
    
    # Replace "문서화" with "Documentation"
    find . -name "*.html" -type f -exec sed -i 's/문서화/Documentation/g' {} \;
    
    # Optional: Replace other Korean terms
    # Uncomment the lines below if you want to replace more Korean terms
    # find . -name "*.html" -type f -exec sed -i 's/메인 페이지/Main Page/g' {} \;
    # find . -name "*.html" -type f -exec sed -i 's/클래스/Classes/g' {} \;
    # find . -name "*.html" -type f -exec sed -i 's/파일들/Files/g' {} \;
    
    echo "Translation fixes applied successfully!"
    cd ..
else
    echo "Error: html directory not found. Please run doxygen first."
    exit 1
fi
