pipeline {
  agent any
  stages {
    stage('OpenFEC') {
      steps {
        sh '''wget http://openfec.org/files/openfec_v1_4_2.tgz
tar -xvf openfec_v1_4_2.tgz && rm openfec_v1_4_2.tgz
mkdir -p openfec_v1.4.2/build && cd openfec_v1.4.2/
wget https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch && patch src/CMakeLists.txt ndnrtc-openfec.patch
cd build/
cmake .. -DDEBUG:STRING=OFF
make'''
      }
    }
  }
}