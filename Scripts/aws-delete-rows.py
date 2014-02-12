# Python script to delete all rows with a given Id value.

import boto.dynamodb
import boto.dynamodb.condition as condition

# Modify the values below to set your AWS region and access keys:

# Set this to the AWS region, for example 'us-west-2', 'us-east-1', etc.
REGION = 'us-east-1'
# Copy your AWS access key value below:
AWS_ACCESS_KEY = 'your_access_key'
# Copy your AWS secret access key value below:
AWS_SECRET_ACCESS_KEY = 'your_secret_access_key'
# Set the name of your AWS cloud thermometer table below:
TABLE_NAME = 'Temperatures'
# Set to the name of the Id value to delete.  All rows which have this ID will be deleted!
ID_TO_DELETE = 'id_value_to_delete'

# Connect to the table.
conn = boto.dynamodb.connect_to_region(REGION, aws_access_key_id=AWS_ACCESS_KEY, aws_secret_access_key=AWS_SECRET_ACCESS_KEY)
temperatures = conn.get_table(TABLE_NAME)

# Print a warning
print 'About to delete all rows with Id {0} from table {1} in {2}.'.format(ID_TO_DELETE, TABLE_NAME, REGION)
print 'Are you sure? (y/n)'
response = raw_input().upper()
if response != 'Y' and response != 'YES':
	print 'OK, not deleting anything!'
	quit()

# Scan the table and delete all items which match the provided ID
print 'Deleting rows...'
count = 0
for row in temperatures.scan(scan_filter={'Id': condition.EQ(ID_TO_DELETE)}):
	count += 1
	row.delete()

print 'Deleted {0} rows.'.format(count)