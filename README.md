# led-memory-game
### Simple GPIO game for embedded systems

Setup:
- In your [buildroot-...](https://buildroot.org/download.html)/packages folder, make a symlink to ./br2-package named led-memory-game
- Back in the top level buildroot-... folder, [`make *config`](https://buildroot.org/downloads/manual/manual.html#configure)
- select the game under Target Packages -> Games -> led-memory-game
- `make`

NB: remember to clone with [`--recurse-submodules`](https://git-scm.com/book/en/v2/Git-Tools-Submodules)!

