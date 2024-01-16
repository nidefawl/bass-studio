import os
import subprocess


gitCommandPerDay = r'git log --date=short --pretty=format:%ad | sort | uniq -c'
# gitCommandPerMonth = r'git log --date=short --pretty=format:%ad | sort | uniq -c | cut -c 1-7 | sort | uniq -c | sort -n'
gitCommandPerMonth = gitCommandPerDay
#run in shell

commitsPerMonth = {}

def runscript(script, args=[], pathAdd=None, shellEnv={'_': ''}):
  if pathAdd: 
    shellEnv=os.environ
    shellEnv['PATH'] = pathAdd + ';' + shellEnv['PATH']
  proc = subprocess.run([r'bash', '-c', script, '--', *args], env=shellEnv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
  print('EXIT CODE', proc.returncode)
# print(proc.stdout)
# print(proc.stderr)
  # get stdout in string variable
  output = proc.stdout
  # iterate per lien
  for line in output.splitlines():
    # line is in format "  1 2019-01-01"
    # split in numCommits and date+year
    trimmed = line.strip()
    splitted = trimmed.split(' ')
    numCommits = splitted[0]
    date = splitted[1]
    # split date in year and month
    splitted = date.split('-')
    year = splitted[0]
    month = splitted[1]
    # create key
    key = year + '-' + month
    # add to dict
    if key in commitsPerMonth:
      commitsPerMonth[key] += int(numCommits)
    else:
      commitsPerMonth[key] = int(numCommits)
  
  print("Per month:")
  # print result
  for key in sorted(commitsPerMonth):
    print(key + ' ' + str(commitsPerMonth[key]))

  # also calculate total commits per year
  commitsPerYear = {}
  for key in sorted(commitsPerMonth):
    splitted = key.split('-')
    year = splitted[0]
    if year in commitsPerYear:
      commitsPerYear[year] += commitsPerMonth[key]
    else:
      commitsPerYear[year] = commitsPerMonth[key]
  
  print("Per year:")
  # print result
  for key in sorted(commitsPerYear):
    print(key + ' ' + str(commitsPerYear[key]))
  
  # render graph using python matplotlib
  import matplotlib.pyplot as plt
  import numpy as np
  import matplotlib.dates as mdates
  import datetime
  
  # create data
  x = []
  y = []
  for key in sorted(commitsPerMonth):
    splitted = key.split('-')
    year = splitted[0]
    month = splitted[1]
    x.append(datetime.date(int(year), int(month), 1))
    y.append(commitsPerMonth[key])
  
  # create plot
  fig, ax = plt.subplots()
  ax.plot(x, y)

  # format the ticks
  ax.xaxis.set_major_locator(mdates.YearLocator())
  ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y'))
  # ax.xaxis.set_minor_locator(mdates.MonthLocator())
  # ax.xaxis.set_minor_formatter(mdates.DateFormatter('%m'))

  # round to nearest years.
  datemin = np.datetime64(x[0], 'Y')
  datemax = np.datetime64(x[-1], 'Y')  + np.timedelta64(1, 'Y')
  ax.set_xlim(datemin, datemax)

  # format the coords message box
  ax.format_xdata = mdates.DateFormatter('%Y-%m-%d')

  # format the coords message box
  ax.format_ydata = lambda x: '%1.2f' % x
  ax.grid(True)

  # rotates and right aligns the x labels, and moves the bottom of the
  # axes up to make room for them
  fig.autofmt_xdate()
  # add title
  plt.title('Git commits per month')

  plt.show()

  return proc.returncode


def main():
    runscript(gitCommandPerMonth)

if __name__ == '__main__':
    main()