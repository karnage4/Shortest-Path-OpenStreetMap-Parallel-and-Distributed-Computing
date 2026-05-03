# # Use an official Ubuntu 22.04 base image
# FROM ubuntu:22.04

# # Avoid prompts from apt during package installations
# ENV DEBIAN_FRONTEND=noninteractive

# # Install dependencies: Python 3, Tkinter, CMake, C++ compiler (GCC), and OpenMP
# RUN apt-get update && apt-get install -y \
#     python3 \
#     python3-pip \
#     python3-tk \
#     build-essential \
#     cmake \
#     libgomp1 \
#     && rm -rf /var/lib/apt/lists/*

# # Set the working directory in the container
# WORKDIR /app

# # Copy the current directory contents into the container at /app
# COPY . /app

# # Create the build directory, run CMake, and compile the C++ project
# RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

# # Make sure gui.py is executable
# RUN chmod +x gui.py

# # Set the command to run the Python GUI
# CMD ["python3", "gui.py"]


# Web browser method
# Use a base image that already has a web-based desktop environment built-in
FROM lscr.io/linuxserver/webtop:ubuntu-xfce

# Install our required dependencies
RUN apt-get update && apt-get install -y \
    python3 python3-pip python3-tk \
    build-essential cmake libgomp1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

# Build the C++ backend
RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)
RUN chmod +x gui.py

# Create an auto-start script so the GUI opens immediately when the desktop loads
RUN mkdir -p /config/.config/autostart
RUN echo "[Desktop Entry]\nType=Application\nExec=python3 /app/gui.py\nPath=/app\nHidden=false\nNoDisplay=false\nX-GNOME-Autostart-enabled=true\nName=OSM_Routing" > /config/.config/autostart/osm.desktop
