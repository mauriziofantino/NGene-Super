# Raphael

This repository is intended to preserve the original code of NGene, a GPS+Galileo receiver that was once top technology but has since fallen into disrepute due to numerous software engineering mistakes and poor decisions.

The project goals are to:

- [ ] Improve the compilation process using automake
- [ ] Structure the repository more intelligently
- [ ] Remove dependencies on Ubuntu (Linux is not just Ubuntu, and Ubuntu is not the best Linux)
- [ ] Convert the receiver to support I/Q sample processing
- [ ] Reengineer the receiver core and address poor thread management
- [ ] Eliminate the double buffer and Linux message queue (using a message queue to share data between different threads is impractical)
- [ ] Add support for frontends designed after the year 2000

** Dependencies **

The software receivers requires two main libraries:

- ncurses which can be installed as you prefer as system wide library using your package manager (apt, emerge, pacman, ect...)
- libfec which are indeed statically linked to the main executable, for that you have to launch the following command after cloning your repo: '''git submodule update --init --recursive'''

** Building Instruction **

''' autoreconf -i '''
''' ./configure '''
''' make '''
