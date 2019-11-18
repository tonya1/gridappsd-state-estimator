FROM gridappsd/gridappsd_base:master

WORKDIR /gridappsd

RUN apt-get update \
    && apt-get install -y \
       m4 \
       liblapack-dev \
       libblas-dev \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /var/cache/apt/archives/*

ENV GRIDAPPSD=/gridappsd
ENV FNCS_INSTALL=${GRIDAPPSD}
ENV GLD_INSTALL=${GRIDAPPSD}
ENV CZMQ_VERSION 4.2.0
ENV ZMQ_VERSION 4.3.1
ENV TEMP_DIR=/tmp/source

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib


# ----------------------------------------------------
# INSTALL State Estimator
# ----------------------------------------------------

RUN cd $TEMP_DIR \

RUN cd $TEMP_DIR \
    && git clone https://github.com/gridappsd/gridappsd-state-estimator.git -b develop --single-branch \
    && cd gridappsd-state-estimator/state-estimator \
    && git clone https://github.com/gridappsd/json.git \
    && cd ${TEMP_DIR}/gridappsd-state-estimator/state-estimator \
    && git clone https://github.com/gridappsd/SuiteSparse.git -b v5.6.0 --single-branch \
    && cd SuiteSparse \
    && make LAPACK=-llapack BLAS=-lblas 
#\
#    && cd ${TEMP_DIR}/gridappsd-state-estimator/state-estimator \
#    && make 


    #\
    #&& cd /tmp \
    #&& /bin/rm -r ${TEMP_DIR}/gridlab-d


