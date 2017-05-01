# Usage: 
#      . compeer.bash
#      source compeer.bash
#
# Add to ~/.bashrc:
# echo 'source $HOME/compeer/compeer.bash' >> $HOME/.bashrc

unset ENCFLAG
# Set to 0 to disable.
ENCRYPT=1

CPP_FILES=$(git ls-files *.cpp | grep -Fv 'peer.cpp')
[[ "$ENCRYPT" -eq 1 ]] && ENCFLAG=-encrypt

alias cml='./compeer -listen localhost 20000 $ENCFLAG'
alias cmf='./compeer -friend localhost 20000 $ENCFLAG'
alias mkcm='rm -f compeer; g++ peer.cpp crypto.cpp util.cpp -o compeer -Wall -lcrypto'
