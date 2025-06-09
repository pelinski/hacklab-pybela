FROM pelinski/xc-bela-container:v1.1.0

COPY docker-scripts/download_libtorch.sh ./
RUN ./download_libtorch.sh  && rm download_libtorch.sh

# rev C
RUN git clone https://github.com/BelaPlatform/bb.org-overlays.git /sysroot/opt/bb.org-overlays

RUN apt-get update && \
      apt-get install -y python3 pip

RUN pip install pybela==2.0.3
RUN pip install torch --index-url https://download.pytorch.org/whl/cu117 

RUN mkdir -p /root/hacklab

RUN git clone --recurse-submodules -j8  https://github.com/pelinski/hacklab-pybela /root/code/ 

WORKDIR /root/hacklab

CMD /bin/bash