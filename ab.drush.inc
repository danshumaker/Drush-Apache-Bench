<?php 
/**
 * @file
 *   Apache Bench Drush Drupal Testing
 *   Author: Dan Shumaker
 *   Date: 1/6/2012
 * Expanded from 2bits:  http://2bits.com/articles/using-apachebench-benchmarking-logged-users-automated-approach.html
 * Related: http://drupal.org/node/659974
 *
 */

/*
 * Implementation of hook_drush_command()
 */
function ab_drush_command()  {
  $items = array();

  $items['ab'] = array(
    'description' => "Run Apache Bench on a drupal site.  ",
    'options' => array(
      'curr' => 'Number of concurrent processes to spawn (determines how many urls will be hit) (default 1)',
      'reqs' => 'Number of requests per concurrent process to spawn (default 1)',
      'nu' => 'Get and use random node urls instead of the home url of the drupal site. (default false)',
      'sess' => 'Use authentication sessions to hit pages that only logged in users can see (default false)',
      'rep' => 'Output statistics to a report file',
      'apache_auth' => 'give apache authentication if necessary. In the form "user:password"  with colon in string',
      'timeout' => 'Timeout in seconds for requests.  Default to 1000 seconds.',
      'protocol' => 'The TCP protocol (either http or https), default http',
    ),
    'examples' => array(
      'drush ab -r /var/www/cwd -l http://cwd/ --curr=4 --reqs=10 --sess' => 'Run apache bench four times with 10 requests per url, using valid user sessions.',
      'drush ab -l http://cwd/ --curr=4 --reqs=10 --sess --nu' => 'Run apache bench four times with four different node urls with 10 requests per url, using valid user sessions',
    ),
  'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL, // No bootstrap at all.
  );

  /* Commandfiles may also add topics.  These will appear in
  // the list of topics when `drush topic` is executed.
  // To view this topic, run `drush --include=/full/path/to/examples topic`
  $items['sandwich-exposition'] = array(
    'description' => 'Ruminations on the true meaning and philosophy of sandwiches.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'callback' => 'drush_print_file',
    'callback arguments' => array(dirname(__FILE__) . '/sandwich-topic.txt'),
  ); 
   */
  return $items;
 
} 

/*
 * Implementation of hook_drush_help()
 *
 */
function ab_drush_help($section) {
  switch ($section) {
    case 'drush:ab':
      return dt("Apache Bench");
    // The 'title' meta item is used to name a group of
    // commands in `drush help`.  If a title is not defined,
    // the default is "All commands in ___", with the
    // specific name of the commandfile (e.g. sandwich).
    // Command files with less than four commands will 
    // be placed in the "Other commands" section, _unless_
    // they define a title.  It is therefore preferable
    // to not define a title unless the file defines a lot
    // of commands.
    case 'meta:ab:title':
      return dt("Apache Bench");
    // The 'summary' meta item is displayed in `drush help --filter`,
    // and is used to give a general idea what the commands in this
    // command file do, and what they have in common.
    case 'meta:ab:summary':
      return dt("Runs Apache Bench on a drupal site");
  }

}
/**
 * Implementation of drush_hook_COMMAND_validate().
 *
 */
function drush_ab_validate() {
/*
  $name = posix_getpwuid(posix_geteuid());
  if ($name['name'] !== 'root') {
    return drush_set_error('MAKE_IT_YOUSELF', dt('What? Make your own sandwich.'));
  }
 */
}

/**
 * Implementation of drush_hook()
 */
function drush_ab() {
    $curr = drush_get_option('curr', 1);
    $reqs = drush_get_option('reqs', 1);
    $sess = drush_get_option('sess', 0);
    $rep = drush_get_option('rep', '');
    $nu = drush_get_option('nu', 0);
    $timeout = drush_get_option('timeout', 1000);
    //$apache_auth = drush_get_option('apache_auth', 'dev:dev1234');
    $apache_auth = drush_get_option('apache_auth', '');
    $protocol = drush_get_option('protocol', 'http');

    $context = drush_get_context();
    //dlm($context);
    //$url = $context['site-aliases']['@self']['uri'];
    if ( !strstr($context['DRUSH_URI'], 'http') ) {
      $url  = $protocol . "://" . $context['DRUSH_URI'] . '/';
    } else {
      $url = $context['DRUSH_URI'];
    }

    if ($apache_auth !== '') {
        $apache_auth = ' -A ' . $apache_auth;
    }

    $dt = date('M_d_Y_g:i:s');
    if ($rep !== '') {
      $rep = getcwd() .'/Apache_Bench_' . $dt . '_concurrent_' . $curr . 'requests_' . $reqs . '_'. $rep . '.txt';
      //$rep = 'Apache_Bench_' . $dt . '_' . $url . '_concurrent_' . $curr . 'requests_' . $reqs . '.txt';
    }

    // Build initial apache bench command that will remain the same
    $abc = array();
    $abc_init ="ab -t " . $timeout . " " . $apache_auth . " -r -c 1 -n " . $reqs;

    for ($i=0; $i<$curr; $i++ ) {
        $abc[$i] = $abc_init;
    }

    if ( $sess ) {
        dlm($sess);
        $result = db_query('SELECT sid FROM {sessions} WHERE uid > 1 LIMIT %s', $curr);
        $i = 0;
        while($row = db_fetch_object($result)) {
            dlm("get sessions");
            $cookie = session_name() .'='. $row->sid;
            $abc[$i] .= " -C " . $cookie ;
            $i++;
        }
    }

    if ( $nu ) {
        $result = db_query('select nid from {node} where status = 1 order by rand() limit %s', $curr);
        $i = 0;
        while($row = db_fetch_object($result)) {
            dlm("getting a node");
            $nurl = $url .'node/'. $row->nid;
            $abc[$i] .= " " . $nurl;
            $i++;
        }
    } else {
        for ($i=0; $i<$curr; $i++ ) {
            $abc[$i] .= " " . $url;
        }
    }

    $output_info = array();
    foreach ($abc as $cmd) {
        $output_info[] = run_in_background($cmd);
    } 

    //drush_print_r($output_info);
    $running = TRUE;
    while($running) {
        sleep(1);
        print "Processes running: ";
        foreach($output_info as $key => $io ) {
            if (is_process_running($io['pid'])) {
              printf(" %s ", $io['pid']); 
            } else { 
              if ( $io['running'] ) {
                printf("\n\n%s Finished command: %s\n", $io['pid'], $io['cmd']);
                $output = file_get_contents($io['file']);
                if ($rep) {
                  file_put_contents($rep, $output, FILE_APPEND);       
                }else {
                  print $output;
                }
                $output_info[$key]['running'] = FALSE;
              }
            }
        }
        print "\n";
        $running = count(array_filter( $output_info, "running_proc" ));
    } 
    if ($rep) {
      print "\n\nOutput reported to " . $rep . "\n";
    }
}

function running_proc( $proc ) {
  return is_process_running( $proc['pid'] );
}
function run_in_background($Command) {
    $temp=array_search('uri', @array_flip(stream_get_meta_data($GLOBALS[mt_rand()]=tmpfile()))); 
    $cmd = sprintf("%s > %s 2>&1 & echo $!", $Command, $temp);
    $PID = exec($cmd);
    printf("%s %s\n", $PID, $cmd);
    return array( 'cmd' => $Command, 'pid' => $PID, 'file' => $temp, 'running' => TRUE);
}
function is_process_running($PID) {
    exec("ps $PID", $ProcessState);
    return(count($ProcessState) >= 2);
}


