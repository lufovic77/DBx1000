 apt update
# apt install linux-aws
apt install -y git
 apt install -y unzip
# git clone https://github.com/yuxiamit/dbx1000_logging.git
mkdir dbx1000_logging
cd ~/dbx1000_logging
unzip -o dbx1000_logging.zip -d dbx1000_logging
# git checkout array-lock-free
 apt install -y libboost-dev
 apt install -y libjemalloc-dev
 apt install -y build-essential
 apt install -y numactl
 apt install -y python python3-scipy
 apt install -y iotop sysstat zsh python-matplotlib python3-matplotlib

# cd dbx1000_logging; sudo python scripts/prepareEC2_i3.py 8

# cd dbx1000_logging; python tools/compile.py

# sudo RUNEXPR_OVERRIDE=0 python3 scripts/runExpr.py shortEC2_8YCSB
