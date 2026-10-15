FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    libcurl4-openssl-dev \
    sqlite3 \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the C++ Engine and API Server
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Expose the API server port
EXPOSE 8080

# Run the API daemon
CMD ["./build/server/utpd"]
