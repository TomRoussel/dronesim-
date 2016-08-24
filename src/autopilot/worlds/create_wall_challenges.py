#!/usr/bin/python
import sys, os, re, shutil
 
# example file that is adapted:
source_world='wall_challenge.world'
# read world file from example
#location= '/home/jay/autopilot_ws/src/autopilot/worlds/'
location='./'
# create destination folder tmp
directory='./wall_challenges_test'
counter = 0
if not os.path.exists(directory):
    os.makedirs(directory)

# description of worlds
description_file_handle = open('./'+directory+'/description.txt','w')

f_sdf = open(location+source_world,'r')
sdf = f_sdf.read()

#for size in xrange(1,15, 1):
for size in xrange(20,200, 2):
    for obstacle in ["wall_e", "wall_w"]:
        expr="<model name=\W"+obstacle+"[\W]+.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*\W[\w]*\W[\W]*\d\.?[\d]*[\s][\W]*\d\.?[\d]*[\s][\W]*\d\.?[\d]*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*\s*.*"
        matches = re.findall(expr, sdf)
        #if len(matches)>1: raise IOError('found more than 1 match. Very confusing :-(')
        expr = "size[\W]*\d\.?[\d]*[\s][\W]*\d\.?[\d]*"
        matches_short = re.findall(expr, matches[0])
        print 'size: ', matches_short[0]
        expr = "size>0.2 "+str(size)
        matches_new=re.sub(matches_short[0],expr,matches[0])
	sdf=re.sub(matches[0],matches_new,sdf)
    # save world file in ./tmp folder
    world_name=directory+'/{:04d}.world'.format(counter)
    new_sdf = open(world_name,'w')
    new_sdf.write(sdf)
    # description
    description = '/{:04d}'.format(counter) + ': '+'wall size: '+str(size)+'\n'
    description_file_handle.write(description)
    counter=counter+1