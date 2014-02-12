# Python script to list all unique ID values in an AWS table
# for the cloud thermometer project.

import boto.dynamodb

# Modify the values below to set your AWS region and access keys:

# Set this to the AWS region, for example 'us-west-2', 'us-east-1', etc.
REGION = 'us-east-1'
# Copy your AWS access key value below:
AWS_ACCESS_KEY = 'your_access_key'
# Copy your AWS secret access key value below:
AWS_SECRET_ACCESS_KEY = 'your_secret_access_key'
# Set the name of your AWS cloud thermometer table below:
TABLE_NAME = 'Temperatures'


# Connect to the table and scan it for all the unique names.
conn = boto.dynamodb.connect_to_region(REGION, aws_access_key_id=AWS_ACCESS_KEY, aws_secret_access_key=AWS_SECRET_ACCESS_KEY)
temperatures = conn.get_table(TABLE_NAME)
# Use a set to remember unique values.
unique_ids = set()
for row in temperatures.scan(attributes_to_get=['Id']):
	unique_ids.add(row['Id'])

# Print all the unique Id values, in sorted order.
print 'Retrieved unique Id values from table {0} in {1}:'.format(TABLE_NAME, REGION)
for name in sorted(unique_ids):
	print name