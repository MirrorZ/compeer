# Usage: 
#      . compeer.bash
#      source compeer.bash
#
# Add to ~/.bashrc:
# echo 'source $HOME/compeer/compeer.bash' >> $HOME/.bashrc

alias mkcm='rm -f compeer; g++ crypto.cpp peer.cpp -o compeer -Wall'
alias cml='./compeer -listen localhost 20000 -encrypt'
alias cmf='./compeer -friend localhost 20000 -encrypt'
