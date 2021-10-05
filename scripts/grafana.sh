docker run -d --net=host --name=grafana -e "GF_INSTALL_PLUGINS=redis-datasource" grafana/grafana
# PORT 3000