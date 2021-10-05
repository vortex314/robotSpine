docker run -d --name elasticsearch --net=host -e "discovery.type=single-node" elasticsearch:6.8.18
docker run -d --name kibana --net=host -e "ELASTICSEARCH_HOSTS=http://localhost:9200" kibana:6.8.18