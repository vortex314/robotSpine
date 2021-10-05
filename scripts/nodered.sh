mkdir data
docker run -it -d --net=host -v node_red_data:data --name nodered nodered/node-red