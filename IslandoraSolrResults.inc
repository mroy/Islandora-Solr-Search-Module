<?php
/* 
 * contains methods to search solr and display results.  depends on Apache_Solr_Php client.
*/

class IslandoraSolrResults {
  static $facetSeparator = '~';//used to separate facets in url
  static $slashReplacement = '~slsh~';// a pattern used to replace / in url's the slash breaks drupal clean url's
  
  
 public static $SEARCH_CLASS_ADVANCED_SEARCH_NUMBER_FIELDS = 5;
 
 
 public static $facetNames = array(
   'subject_topic_facet' => 'Subject - Topic',
   'type_of_resource_facet' => 'Format',
   'subject_geographic_facet' => 'Subject - Geographic',
   'subject_name_facet' => 'Subject - Name',
   'subject_temporal_facet' => 'Subject -Temporal',
   'name_organization_facet' => 'Organization',
	 'subject_building_type_facet' => 'Building Type',
	 'subject_building_usage_facet' => 'Building Usage',
	 'subject_building_style_facet' => 'Building Style'
 );
 
  function IslandoraSolrResults() {
    module_load_include('php', 'islandora_solr_search', 'Solr/Service');
  }
 
  function parseQuery($solr, $query,$dismax=false) {
    $query = str_replace(IslandoraSolrResults::$slashReplacement,'/',$query);   //can't have a slash in the url breaks drupal clean urls
    return $query;
  }
//default implementation for solr search.  You can configure others in the block config settings
  function searchAndDisplay($query,$startPage=1,$fq=null,$dismax=null, $sort = null, $limit = null, $saveSearch = TRUE) {
    
    
     if ($dismax != null) // store the search query if its a simple keyword search
     {
       $_SESSION['query_string'] = $query;
     }    
    
    if ($limit == NULL) {
      $limit =  variable_get('islandora_solr_search_num_of_results',20); 
    }
    
    global $islandora_solrQuery, $islandora_solr_page_number, $islandora_limit, $base_url;//used by facect block as well
    
    $islandora_limit = $limit;    
    $islandora_solr_page_number = $startPage;
    $islandora_solrQuery = str_replace('/',IslandoraSolrResults::$slashReplacement,$query);//replace the slash so we can use this in a url without breaking drupal clean urls
    
    if(empty($islandora_solrQuery)) {
      $islandora_solrQuery='%20';//so we can allow empty queries to dismax
    }
    // module_load_include('php', 'fedora_repository', 'Solr/Service');
    $host = variable_get('islandora_solr_search_block_host','localhost');
    $port = variable_get('islandora_solr_search_block_port','8080');
    $appName = variable_get('islandora_solr_search_block_app_name','solr');
    $solr = new Apache_Solr_Service($host, $port, '/'.$appName.'/');
    $query = $this->parseQuery($solr,$query,$dismax,$solr);//fix the query as some characters will break the search : and / slash are examples
    
    
    $facetlimit = variable_get('islandora_solr_search_block_facet_limit','12');
    $facetMinCount = variable_get('islandora_solr_search_block_facet_min_count','2');
    $requestHandler = ($dismax!=null)?'dismax':variable_get("islandora_solr_search_block_request_handler",t("standard"));

		$facets = explode(',',variable_get("islandora_solr_search_block_facets",'dc.subject,dc.type'));
		if (isset($_SESSION['fedora_collection']) && $_SESSION['fedora_collection'] == 'uofm:prairieIslamic') { 
			$facets = array('subject_building_type_facet','subject_building_usage_facet','subject_building_style_facet');
		}

    $additionalParams = array(
            'facet' => 'true',
            'facet.mincount' => $facetMinCount,
            'facet.limit' => $facetlimit,
            'qt' => $requestHandler,
            'facet.field' => $facets,//comma separated list configured in the block config
    );
    if ($sort != null) 
    {
      $additionalParams['sort'] = $sort;
			$_SESSION['islandora_solr_sort'] = $sort;
    }	 else 
		{
			$additionalParams['sort'] = 'title_t_sort asc';
		}
    
    global $islandora_fq;
    if($fq != NULL && $fq != '-') {
      $fq = str_replace(IslandoraSolrResults::$slashReplacement,'/',$fq);//put the slash back
      if(strpos($fq,'-')===0) {//remove the requested facet from the list
        $fq = substr($fq,strpos($fq,IslandoraSolrResults::$facetSeparator)+1);
      }
      $fqs = $this->csv_explode(IslandoraSolrResults::$facetSeparator,$fq,'"',true);//to filter by more then one facet we will separate them by~ for now
      $additionalParams['fq'] = $fqs;
      $islandora_fq = str_replace('/',IslandoraSolrResults::$slashReplacement,$fq);//remove the slash here as we will be using this in url's
    }
    if(empty($islandora_fq)) {
      $islandora_fq = '-';
			$additionalParams['fq'][0]='-cmodel_t:compound';
    } else {
  		$additionalParams['fq'][]= '-cmodel_t:compound';
    }

    global $islandora_defType;
    if($dismax != NULL) {
      $islandora_defType = $dismax;
      $additionalParams['defType'] =$dismax;
    }

		if ($saveSearch) {
	    $_SESSION['islandora_solr_page_number'] = $islandora_solr_page_number;
  	  $_SESSION['islandora_solr_limit'] = $islandora_limit;
    	$_SESSION['islandora_solr_query'] = $islandora_solrQuery;
	    $_SESSION['islandora_solr_page_number'] = $islandora_solr_page_number;
			$_SESSION['islandora_solr_fq'] = $islandora_fq;
			$_SESSION['islandora_solr_defType'] = $islandora_defType;  
 		}

    $recordStart = $startPage -1;
    $recordStart = max(0,$recordStart);//don't want a negative number
    $recordStart = $recordStart * $limit;
    
    try {
      $results = $solr->search($query, $recordStart, $limit,$additionalParams);
    }catch (Exception $e) {
      drupal_set_message(t('error searching ').$e->getMessage());
    }
    if(empty($results)){
      drupal_set_message(t('Error searching solr index. Is the solr search block configured properly?'),'error');
      return ' ';
    }
    $total = (int) $results->response->numFound;   
    $start = min(1, $total);
    $end = min(($limit + $recordStart), $total);
    
    global $islandora_endPage;
    $islandora_endPage = ceil($total/$limit);
    //if user requested a page past the end.. fix the start page and try again... bad user!
    if ($startPage > $islandora_endPage)
    {
      $startPage = $islandora_endPage;
      $recordStart = $startPage -1;
      $recordStart = max(0,$recordStart);//don't want a negative number
      $recordStart = $recordStart * $limit;
      
      try {
	$results = $solr->search($query, $recordStart, $limit,$additionalParams);
      }catch (Exception $e) {
	drupal_set_message(t('error searching ').$e->getMessage());
      }
      if(empty($results)){
	drupal_set_message(t('Error searching solr index. Is the solr search block configured properly?'),'error');
	return ' ';
      }      
    }
    $islandora_startPage = $startPage;
    
    $output .='<div class="searchDesc"><h2>Search Results - </h2> Results '.($recordStart + 1).' - '. $end .' of '. $total.  ($dismax!=null?' for the keyword(s) <b>"'.$query.'"</b>':'').'</div>';
    $pager='<div class = "pager">';//opening div for paging links
		$pager.='Page '.$startPage.' of '.$islandora_endPage.' &nbsp;&nbsp;';
    
		$pager .='<a href="?page=1&fq='.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'">[1]</a>&nbsp;';
		$i=max(2,$startPage-3);
		if ($i > 2) { 
			$pager .= '...&nbsp;';
		}
		for (; $i< min($startPage+3,$islandora_endPage); $i++) {
			if ($i != $startPage) {
				$pager .= '<a href="?page='.$i.'&fq='.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'">['.($i).']</a>&nbsp;';
			} else {
				$pager.= '['.($i).']&nbsp;';
			}
		} 
  	if ($i < $islandora_endPage-1) {
			$pager .= '...&nbsp;';
		}
		
		if ($islandora_endPage > 1) { 
			$pager .='<a href="?page='.($islandora_endPage).'&fq='.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'">['.($islandora_endPage ).']</a>';
		}
    
    $pager .= '</div>';//added closing div for drupal paging here
    $output.=$pager;

    //call a function defined in the blocks config
    $solrFile = trim(variable_get('islandora_solr_search_block_handler_file','IslandoraSolrResults.inc'));
    if(strpos($solrFile,'../')){//don't let us bust out of islandora_solr_search modules directory when looking for a handler
      drupal_set_message(t('You have illegal characters in your solr handler function in the Islandora solr block config.'), 'error');
      return ' ';
    }
    $solrClass = trim(variable_get('islandora_solr_search_block_handler_class','SolrResults'));
    $solrFunction = trim(variable_get('islandora_solr_search_block_handler_function','SearchAndDisplay'));
   require_once(drupal_get_path('module', 'islandora_solr_search') . '/'. $solrFile);
    //try {
    
      if(class_exists($solrClass)){
      $implementation = new $solrClass();
      } else {
         drupal_set_message(t('Error loading solr results class. Class could not be loaded. Check the Islandora Solr search blocks configuration.'),'error');
         return ' ';
      }
    //} catch (Exception $e) {
    //  watchdog(t("Fedora_Repository"), t("Error Getting Solr Search Results Class!") . $e->getMessage(), NULL, WATCHDOG_ERROR);
   //   return 'Error getting Solr Search Results class.  Check watchdog for more info.';
   // }
    if(method_exists($implementation,$solrFunction)){
       $output.= $implementation->$solrFunction($results);
      } else {
        drupal_set_message(t('Error loading solr results class. Could not find the specified method.  Check the Islandora Solr search blocks configuration.'),'error');
      }
    global $islandora_facets;//make facets available to the islandora facet block
    $islandora_facets = $results->facet_counts->facet_fields;
 		$output .= '<div class="block block-islandora_solr_search"><h2>'.t('Modify Your Search').'</h2>';
		$output .= $this->displayFacets();
		$output .= '</div><br clear="all"/>';
		$output .= $pager; 
    return $output;
  }
//default implementation of display results can be changed in the blocks config
  function displayResults($results){
      $output.= '<ol start="'.($recordStart + 1).'">';
    global $base_url;
    if(empty($results)) {
      return "no results";
    }

    foreach($results->response->docs as $doc) {
      $output .= '<li><table>';
      foreach($doc as $field => $value) {
        $output .= '<tr><th>'.$field.'</th>';
        if(is_array($value)) {
          $value = implode(", ",$value);
        }

        if($field=='PID') {
          $output.='<td><a href="'.$base_url.'/fedora/repository/'.htmlspecialchars($value,ENT_QUOTES,'utf-8').'">'.$value.'</td>';
        }else {
          $output .= '<td>'.$value.'</td>';
        }
      }
      $output .='</table></li>';
    }
    $output .= '</ol>';
    return $output;
  }
  
  
//called from the islandora facet block
  function displayFacets() {
    global $islandora_facets, $islandora_defType, $islandora_solrQuery, $base_url, $islandora_fq;
    
    
    $output = drupal_get_form('islandora_solr_modify_search_form');
    
    $output .= '<div class = "facet-list">';
    $output .='<ul>';
    if(empty($islandora_facets)) {
      return ''; //no facets to show
    }

    foreach($islandora_facets as $key => $field) {
      $test = get_object_vars($field);//get the number of fields if there aren't any don't show the key
      if (count($test)>0) {
        $output .='<li><strong>'. htmlspecialchars(isset(IslandoraSolrResults::$facetNames[$key])?IslandoraSolrResults :: $facetNames[$key]:$key, ENT_QUOTES, 'utf-8').'</strong></li>';
        $output .='<ul>';
        $filter_query;
        foreach($field as $name=>$number) {
          if($islandora_fq && $islandora_fq != '-') {//there are existing facets in the query
            $disable_link = strstr($islandora_fq,$key.':"'.str_replace('/',IslandoraSolrResults::$slashReplacement,$name).'"');//we don't want a link for this facet as we already used it
            if($disable_link) { //don't show link to this facet but include a link to remove it
              $edited_list = str_replace($key.':"'.str_replace('/',IslandoraSolrResults::$slashReplacement,$name).'"', '', $islandora_fq);//remove the filter query from the list
              $filter_query = '-'.$key . ':' . htmlspecialchars('"'.drupal_urlencode($name).'"',ENT_QUOTES,'utf-8',false). IslandoraSolrResults::$facetSeparator . htmlspecialchars(drupal_urlencode($edited_list),ENT_QUOTES,'utf-8',false);
            } else {
              $filter_query = $key . ':' . htmlspecialchars('"'.drupal_urlencode($name).'"',ENT_QUOTES,'utf-8',false). IslandoraSolrResults::$facetSeparator . htmlspecialchars(drupal_urlencode($islandora_fq),ENT_QUOTES,'utf-8',false);
            }
          } else { //no existing facets chosen
            $filter_query = $key . ':' . htmlspecialchars('"'.drupal_urlencode($name).'"',ENT_QUOTES,'utf-8');
          }
          $filter_query = str_replace('/',IslandoraSolrResults::$slashReplacement,$filter_query);//replace the slash so url does not break
          if($disable_link) {//creat a link to remove this facet as it has already been chosen
            $output .='<li><div>'.$name . ' (' . $number.')'; //no href here
            //href here if we wanted an image or something for a facet undo we would put it here
            $output .=' '.'<a href="?fq='.$filter_query.'&page=1"/><img src="'.$baseUrl.'/'.drupal_get_path('module','islandora_solr_search').'/images/minus.gif" alt="remove"/></a> ';
          } else {//normal link
            $output.='<li><div><a href="?fq='.$filter_query.'&page=1"/>'.$name.'</a> ('.$number.') </div></li>';
          }
        }
        $output .='</ul>';
      }
    }
    $output.='</ul>';
    $output .= '</div>';
    return $output;
  }
//a better explode method allows quotes in the returned strings.  taken from the php.net site
  function csv_explode($delim=',', $str, $enclose='"', $preserve=false) {
    $resArr = array();
    $n = 0;
    $expEncArr = explode($enclose, $str);
    foreach($expEncArr as $EncItem) {
      if($n++%2) {
        array_push($resArr, array_pop($resArr) . ($preserve?$enclose:'') . $EncItem.($preserve?$enclose:''));
      }else {
        $expDelArr = explode($delim, $EncItem);
        array_push($resArr, array_pop($resArr) . array_shift($expDelArr));
        $resArr = array_merge($resArr, $expDelArr);
      }
    }
    return $resArr;
  }



  function build_solr_search_form($repeat = NULL, $query = NULL) 
  {
        
    module_load_include('inc', 'fedora_repository', 'CollectionPolicy');
    $collection = isset($_SESSION['fedora_collection'])?$_SESSION['fedora_collection']:variable_get('fedora_repository_pid', 'islandora:top');

    $types=array();
    if (($cp = CollectionPolicy::loadFromCollection($collection))!==FALSE)
    {
      $terms = $cp->getSearchTerms(TRUE,TRUE);

      foreach ($terms as $term)
      {
	 $types[$term['field']]=$term['value'];
      }
    }    

    $queryArray=NULL;
    if (isset($query)) {
      $queryArray = explode('AND', $query);
    }

    $andOrArray = array(
            'AND' => 'and',
            'OR' => 'or' //removed or for now as it would be a pain to parse
    );
    $form = array();
    $form['#multistep'] = TRUE; // force the form to be rebuilt every page so that if the collection changes, so do the search terms.
    
    if (!isset($repeat)) {
      $repeat = variable_get('islandora_solr_search_block_repeat',  t('3'));
    }
    $var0 = explode(':', $queryArray[0]);
    $var1 = explode(':', $queryArray[1]);
    $form['search_type']['type1'] = array(
            '#title' => t(''),
            '#type' => 'select',
            '#options' => $types,
            '#default_value' => trim($var0[0])
    );
    $form['fedora_terms1'] = array(
            '#size' => '24',
            '#type' => 'textfield',
            '#title' => t(''),
            '#required' => TRUE,
            '#default_value' => (count($var0) >= 2 ? trim($var0[1]) : ''),
    );
    $form['andor1'] = array(
            '#title' => t(''),
            '#type' => 'select',
            '#default_value' =>  'AND',
            '#options' => $andOrArray
    );
    $form['search_type']['type2'] = array(
            '#title' => t(''),
            '#type' => 'select',
            '#options' => $types,
            '#default_value' => (count($var1) >= 2 ? trim($var1[0]) : ''),
    );
    $form['fedora_terms2'] = array(
            '#size' => '24',
            '#type' => 'textfield',
            '#title' => t(''),
            '#default_value' => (count($var1) >= 2 ? $var1[1] : ''),
    );
    if ($repeat>2 && $repeat < 9) { //don't want less then 2 or more then 9
      for ($i = 3; $i < $repeat + 1; $i++) {
        $t = $i - 1;
        $field_and_term = explode(':', $queryArray[$t]);
        $form["andor$t"] = array(
                '#title' => t(''),
                '#type' => 'select',
                '#default_value' => 'AND',
                '#options' => $andOrArray
        );
        $form['search_type']["type$i"] = array(
                '#title' => t(''),
                '#type' => 'select',
                '#options' => $types,
                '#default_value' => trim($field_and_term[0])
        );
        $form["fedora_terms$i"] = array(
                '#size' => '24',
                '#type' => 'textfield',
                '#title' => t(''),
                '#default_value' => (count($field_and_term) >= 2 ? trim($field_and_term[1]) : ''),
        );
      }
    }

    $form['submit'] = array(
            '#type' => 'submit',
            '#value' => t('search')
    );
    return $form;
  }

  function build_simple_solr_form(){
   
    
    //$form = array();
    $form["search_query"] = array(
                '#size' => '22',
                '#type' => 'textfield',
                '#title' => t(''),
                '#default_value'=> isset($_SESSION['query_string'])?$_SESSION['query_string']:'',
               // '#default_value' => (count($field_and_term) >= 2 ? trim($field_and_term[1]) : ''),
        );
    $form['submit'] = array(
            '#type' => 'image_button',
            '#value' => t('search'),
            '#src' =>  drupal_get_path('module', 'islandora_solr_search') . '/search.gif',
    );
    
    if (isset($_SESSION['query_string'])) 
    {
      unset($_SESSION['query_string']);
    }
    
    return $form;
  }
  function theme_solr_search_form($form) {
    if (!isset($repeat)) {
      $repeat = variable_get('islandora_solr_search_block_repeat',  t('3'));
    }

    $output = drupal_render($form['search_type']['type1']) ;
    $output .= drupal_render($form['fedora_terms1']) ;
    $output .=  drupal_render($form['andor1'])  . drupal_render($form['search_type']['type2']) ;
    $output .= drupal_render($form['fedora_terms2']);
    if ($repeat>2 && $repeat < 9) {
      for ($i=3;$i<$repeat+1;$i++) {
        $t = $i - 1;
        $output .=  drupal_render($form["andor$t"])  . drupal_render($form['search_type']["type$i"]) ;
        $output .= drupal_render($form["fedora_terms$i"]) ;
      }
    }
    $output .=  drupal_render($form['submit']) ;
    $output .= drupal_render($form);
    return $output;


  }

  function mapsDisplay($results) {
 $output.= '<ol start="'.($recordStart + 1).'">';
    global $base_url;
    if(empty($results)) {
      return "no results";
    }

    foreach($results->response->docs as $doc) {
      $output .= '<li><table>';
      $rowspan = 0;
      $pidValue = null;
      $output .= '<tr>';
       $output .= '<td ><img src="'.$base_url.'/fedora/repository/'.htmlspecialchars($doc->PID,ENT_QUOTES,'utf-8').'/TN"/></td>';
       $output .= '<td><table>';
      foreach($doc as $field => $value) {
        $rowspan++;
         $output .= '<tr><th>'.$field.'</th>';

        if(is_array($value)) {
          $value = implode(", ",$value);
        }
        if($field=='PID') {
          $pidValue=$value;
          $output.='<td><a href="'.$base_url.'/fedora/repository/'.htmlspecialchars($value,ENT_QUOTES,'utf-8').'">'.$value.'</td></tr>';
        }else {
          $output .= '<td>'.$value.'</td></tr>';
        }
      }

      $output .='</table></td></tr></table></li>';
    }
      $output .= '</ol>';
      return $output;
  }

 
//this function will be removed.  class has been refactored so we only need the display loop.  leaving here until mnpl is updated.
 function mnplSearchAndDisplay($query,$startPage=1,$fq=null,$dismax=null) {

      $limit =  variable_get('islandora_solr_search_num_of_results',20);
    global $islandora_solrQuery, $islandora_solr_page_number, $base_url;//used by facect block as well
      $islandora_solr_page_number = $startPage;
      $islandora_solrQuery = str_replace('/',IslandoraSolrResults::$slashReplacement,$query);//replace the slash so we can use this in a url without breaking drupal clean urls
    if(empty($islandora_solrQuery)){
      $islandora_solrQuery='%20';//so we can allow empty queries to dismax
    }
    // module_load_include('php', 'fedora_repository', 'Solr/Service');
      $host = variable_get('islandora_solr_search_block_host','localhost');
      $port = variable_get('islandora_solr_search_block_port','8080');
      $appName = variable_get('islandora_solr_search_block_app_name','solr');
      $solr = new Apache_Solr_Service($host, $port, '/'.$appName.'/');
      $query = $this->parseQuery($solr,$query,$dismax,$solr);//fix the query as some characters will break the search : and / slash are examples
      $facetlimit = variable_get('islandora_solr_search_block_facet_limit','12');
      $facetMinCount = variable_get('islandora_solr_search_block_facet_min_count','2');
      $requestHandler = variable_get("islandora_solr_search_block_request_handler",t("standard"));
      $additionalParams = array(
            'facet' => 'true',
            'facet.mincount' => $facetMinCount,
            'facet.limit' => $facetlimit,
            'qt' => $requestHandler,
            'facet.field' => explode(',',variable_get("islandora_solr_search_block_facets",'dc.subject,dc.type')),//comma separated list configured in the block config
    );
    global $islandora_fq;
    if($fq != NULL && $fq != '-') {
      $fq = str_replace(IslandoraSolrResults::$slashReplacement,'/',$fq);//put the slash back
      if(strpos($fq,'-')===0){//remove the requested facet from the list
      $fq = substr($fq,strpos($fq,IslandoraSolrResults::$facetSeparator)+1);
      }
      $fqs = $this->csv_explode(IslandoraSolrResults::$facetSeparator,$fq,'"',true);//to filter by more then one facet we will separate them by~ for now
      $additionalParams['fq'] = $fqs;
      $islandora_fq = str_replace('/',IslandoraSolrResults::$slashReplacement,$fq);//remove the slash here as we will be using this in url's
    }
    if(empty($islandora_fq)) {
      $islandora_fq = '-';
    }
    global $islandora_defType;
    if($dismax != NULL) {
      $islandora_defType = $dismax;
      $additionalParams['defType'] =$dismax;
    }
    $recordStart = $startPage -1;
    $recordStart = max(0,$recordStart);//don't want a negative number
    $recordStart = $recordStart * $limit;
    try {
      
      $results = $solr->search($query, $recordStart, $limit,$additionalParams);
    }catch (Exception $e) {
      drupal_set_message(t('error searching ').$e->getMessage());
    }
    $total = (int) $results->response->numFound;
    $start = min(1, $total);
    $end = min(($limit + $recordStart), $total);
    $output .='<div>Results '.($recordStart + 1).' - '. $end .' of '. $total.'</div>';
    $output .='<div class="item-list"><ul class = "pager">';//opening div for paging links
// 

    if($recordStart > 0) {
      $output .='<li class="pager-previous"><a href="'.$base_url.'/fedora/repository/solr_search/'.htmlspecialchars(drupal_urlencode($islandora_solrQuery),ENT_QUOTES,'utf-8',false).'/0/'.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'/'.$dismax.'">‹ first</a></li> ';
      $output .='<li class="pager-previous"><a href="'.$base_url.'/fedora/repository/solr_search/'.htmlspecialchars(drupal_urlencode($islandora_solrQuery),ENT_QUOTES,'utf-8',false).'/'.max(0,($startPage-1)).'/'.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'/'.$dismax.'">‹ previous</a></li> ';
    }
    if($recordStart < ($total-$limit)) {
      $output .='<li class = "pager-next"><a href="'.$base_url.'/fedora/repository/solr_search/'.htmlspecialchars(drupal_urlencode($islandora_solrQuery),ENT_QUOTES,'utf-8',false).'/'.($startPage+1).'/'.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'/'.$dismax.'">next ›</a></li>';
      $output .='<li class = "pager-next"><a href="'.$base_url.'/fedora/repository/solr_search/'.htmlspecialchars(drupal_urlencode($islandora_solrQuery),ENT_QUOTES,'utf-8',false).'/'.ceil($total/$limit).'/'.htmlspecialchars($islandora_fq,ENT_QUOTES,'utf-8',false).'/'.$dismax.'">last ›</a></li>';
    }
    $output .= '</div></ul>';//added closing div for drupal paging here
    //most formatting of results happens in the loop below.  maybe pull this out to a function
    $output.= '<ol start="'.($recordStart + 1).'">';
    global $base_url;
    if(empty($results)) {
      return "no results";
    }
    foreach($results->response->docs as $doc) {
      $output .= '<li>';
      $pidValue='';
      $genusValue='';
      $speciesValue='';
      $keyValue;

      foreach($doc as $field => $value) {
        //var_dump($doc);
        if(is_array($value)) {
          $value = implode(" ",$value);
        }
        if($field=='PID') {
          $pidValue=$value;
        }else if ($field=='genus') {
          $genusValue=$value;
        }else if ($field=='specimen.species') {
          $speciesValue=$value;
        }else if ($field=='lab_id') {
          $keyValue=$value;
        }
      }
      $typeString='Specimen';
      if($index=(strpos($pidValue,'-F-',TRUE))) {
        $pidForImage = substr($pidValue,0,$index);
        $pidForImage = str_replace('fraction','specimen',$pidForImage);
        $typeString='Fraction';
      }else  if($index=(strpos($pidValue,'-C-',TRUE))) {
        $pidForImage = strstr($pidValue,0,$index);
        $pidForImage = str_replace('compound','specimen',$pidForImage);
        $typeString='Compound';
      }else {
        $pidForImage = $pidValue;

      }
      $output.='<img src="'.$base_url.'/fedora/repository/'.htmlspecialchars($pidForImage,ENT_QUOTES,'utf-8').'/TN"/><a href="'.$base_url.'/fedora/repository/'.htmlspecialchars($pidValue,ENT_QUOTES,'utf-8').'"><strong>'.$typeString.'</strong>'.$keyValue.': '.$genusValue.' '.$speciesValue.'</a> ';


      $output .='</li>';
    }
    $output .= '</ol>';

    global $islandora_facets;//make facets available to the islandora facet block
    $islandora_facets = $results->facet_counts->facet_fields;
    return $output;
  }


}


?>
