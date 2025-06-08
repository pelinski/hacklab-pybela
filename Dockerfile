FROM pelinski/xc-bela-container:v1.1.0

RUN apt-get update && \
      apt-get install -y python3 pip
            
COPY code/requirements.txt ./
RUN pip install -r requirements.txt

COPY docker-scripts/download_libtorch.sh ./
RUN ./download_libtorch.sh  && rm download_libtorch.sh

# rev C
RUN git clone https://github.com/BelaPlatform/bb.org-overlays.git /sysroot/opt/bb.org-overlays

RUN mkdir -p /root/code

RUN git clone --recurse-submodules -j8  https://github.com/pelinski/hacklab-pybela /root/code/ 

WORKDIR /root/code

CMD /bin/bash