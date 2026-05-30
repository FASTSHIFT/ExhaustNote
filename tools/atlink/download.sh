./ATLink_Console.sh
export LD_LIBRARY_PATH=$(pwd)
./ATLink_Console -device AT32F403AVGT7 -connect -p --dfap --depp -e --all -d --a 08000000 --fn /home/artery/test_binhex/test_64k.bin --v -usd --set --fn /home/artery/test_binhex/UserSystemData.bin -p --efap1
