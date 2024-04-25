#/ob/oceanbase-master

build="build_"
loc1="${build}$1"
loc2="./${loc1}/src/observer/observer"
loc3="/root/.obd/repository/oceanbase-ce/4.1.0.1/d03fafa6fa8ceb0636e4db05b5b5f6c3ac2256a3/bin/"
echo $loc1
echo $loc2
echo $loc3

#build_debug or build_release ob
bash build.sh $1 --init --make -j 6
#cd
cd $loc1
#construct
make -j observer
#cd
cd ../
#stop obcluster
obd cluster stop obcluster
#wait 
sleep 3s
#cp replacement
cp $loc2 $loc3