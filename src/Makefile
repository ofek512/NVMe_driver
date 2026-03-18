CXX = g++
CXXFLAGS = -std=c++17 -O2 -g -Wall -I../spdk/include

PKG_CONF_DIR = ../spdk/build/lib/pkgconfig
SPDK_LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONF_DIR) pkg-config --libs spdk_nvme spdk_env_dpdk)
SPDK_CFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONF_DIR) pkg-config --cflags spdk_nvme spdk_env_dpdk)

TARGET = b2b_nvme_driver
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(SPDK_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(SPDK_CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)