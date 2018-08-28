# Get configuration

## Request
GET: https://api.vapor.cloud/application/applications/:repo/hosting/environments/:env/configurations

### Headers
Authorization: Bearer $(TOKEN)


## Response

[
    {
        "value": "f7860336c0dc4f6cbdbc97b7894221a6.eu-west-1.aws.found.io",
        "key": "ELASTIC_HOSTNAME",
        "environment": {
            "id": "04C2AD4D-0547-4AC8-A15F-5F5364D00D75"
        },
        "id": "09C54CA1-9864-4C93-BBB4-7AEAFD1C07FC"
    },

    ...
]

# Set configuration

## Request
PATCH: https://api.vapor.cloud/application/applications/:repo/hosting/environments/:env/configurations

### Headers
Authorization: Bearer $(TOKEN)

### Body

{
	"key": ""
}

## Response

[
    {
        "value": "value",
        "key": "key",
        "environment": {
            "id": "04C2AD4D-0547-4AC8-A15F-5F5364D00D75"
        },
        "id": "09C54CA1-9864-4C93-BBB4-7AEAFD1C07FC"
    },

    ...
]