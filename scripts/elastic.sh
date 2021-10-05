PUT _ingest/pipeline/add-timestamp
{
   "processors" : [
    {
      "date" : {
        "field" : "timestamp",
        "target_field" : "timestamp",
        "formats" : ["UNIX_MS"],
        "timezone" : "Europe/Amsterdam"
      }
    }
  ]
}
GET _ingest/pipeline

curl -XPUT "http://localhost:9200/_ingest/pipeline/add-timestamp" -H 'Content-Type: application/json' -d'
{
   "processors" : [
    {
      "date" : {
        "field" : "timestamp",
        "target_field" : "timestamp",
        "formats" : ["UNIX_MS"],
        "timezone" : "Europe/Amsterdam"
      }
    }
  ]
}'