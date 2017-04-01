# Usage: 
#      . compeer.bash
#      source compeer.bash
#
# Add to ~/.bashrc:
# echo 'source $HOME/compeer/compeer.bash' >> $HOME/.bashrc

unset ENCFLAG
# Set to 0 to disable.
ENCRYPT=0

CPP_FILES=$(git ls-files *.cpp | grep -Fv 'peer.cpp')
[[ "$ENCRYPT" -eq 1 ]] && ENCFLAG=-encrypt

alias mkcm="rm -f compeer; g++ peer.cpp $CPP_FILES -o compeer -Wall"
alias cml='./compeer -l -i localhost -p 20000 $ENCFLAG'
alias cmf='./compeer -f -i localhost -p 20000 $ENCFLAG'
