<?

class UofMSearchResults
{

 private $model_assigments = array( 'uofm:lowResImage' => 'Image', 'highResImage' => 'Image');


 function lookupMarcrelator($code) {

 $moduleRoot = drupal_get_path('module', 'islandora_form_elements');
 $string=trim($code);

   $dom = DOMDocument::load ( $moduleRoot .'/xml/relators.rdf' );
   $rootEl = $dom->getElementsByTagName('RDF');
   if ($rootEl->length > 0) {
     $rootEl=$rootEl->item(0);
     $xpath = new DOMXPath($dom);
     $xpath->registerNamespace ( 'skos' , $rootEl->getAttribute('xmlns:skos') );
     $xpath->registerNamespace ( 'rdf' , $rootEl->getAttribute('xmlns:rdf') );
		
     $result = $xpath->query('/rdf:RDF/rdf:Description[skos:notation="'. $string .'"]');

     $terms = array();
     foreach ($result as $term) {
       $termVal =$term->getElementsByTagName('prefLabel')->item(0)->nodeValue;
       $termVal = preg_replace('/\s+/', ' ', $termVal);
			 return $termVal;
     }
   }
	return '';
 }

 function displayResults($results){

    global $base_url;
    if(empty($results)) {
      return "no results";
    }
    
    $moduleRoot=drupal_get_path('module','islandora_solr_search');
    drupal_add_css($moduleRoot.'/css/uofm.css');
    drupal_add_css($moduleRoot.'/css/jquery.lightbox-0.5.css');
    drupal_add_js($moduleRoot.'/js/jquery.lightbox-0.5.modified.js');
    drupal_add_js($moduleRoot.'/js/uofm.js');    
    
    drupal_add_js("$(document).ready(function () {  $('.results a.zoomIcon').lightBox( { txtImage: 'Result', imageLoading: '$base_url/$moduleRoot/images/lightbox-icon-loading.gif',  imageBtnClose: '$base_url/$moduleRoot/images/lightbox-btn-close.gif',  imageBtnPrev: '$base_url/$moduleRoot/images/lightbox-btn-prev.gif', imageBtnNext: '$base_url/$moduleRoot/images/lightbox-btn-next.gif'  });  });",'inline');
    
    $output.='<div class="results"/><table border="0" cellpadding="10" cellspacing="0" width="100%">';
		$arg = join('/',arg());
    foreach($results->response->docs as $doc) {


      $pid=htmlspecialchars(preg_replace('/\//',':',$doc->id),ENT_QUOTES,'utf-8');
      
      $output .= '<tr><td><div class="thumbnail"><a href="'.$base_url.'/fedora/'.$pid.'/JPG" class="zoomIcon">&nbsp;</a><a href="'.$base_url.'/fedora/repository/'.$pid.'" class="lightbox" title="'.$doc->title_t.'"><img src="'.$base_url.'/fedora/'.$pid.'/TN"/></a></td>';
      $output .= '<td><div class="metadata"><a href="'.$base_url.'/fedora/repository/'.$pid.'">'.$doc->title_t.'</a><ul>';
      if (isset($doc->subject_temporal_t))
      {
	$output.='<li><b>Date:</b>&nbsp;'.$doc->subject_temporal_t.'</li>';
      }

      if (isset($doc->type_of_resource_t))
      {
	$output.='<li><b>Format:</b>&nbsp;'.$doc->type_of_resource_t.'</li>';
      }      
      
      if (isset($doc->name_personal_t))
      {
				$personalEntities=array();
        if (is_array($doc->name_personal_t)) {
          foreach ($doc->name_personal_t as $org) {
            $this->handleEntity($personalEntities,$org,'Personal');
          }
        } else {
            $this->handleEntity($personalEntities,$doc->name_personal_t,'Personal');
        }
				
				foreach ($personalEntities as $name=>$vals) {
          $output .= '<li><b>'.$name.'</b>:&nbsp;';
					foreach ($vals as $val) {
						$val = trim(preg_replace('/[,\s]+/',' ',$val)); 
						$output.= l($val,'islandora/solr/search/name_personal_t:"'.$val.'"').'&nbsp; ';
					}
					$output .= '</li>';
        }
      }

      if (isset($doc->name_organization_t))
      {
				$orgEntities=array();
				if (is_array($doc->name_organization_t)) { 
					foreach ($doc->name_organization_t as $org) {	
						$this->handleEntity($orgEntities,$org,'Organization');
					}
				} else {
						$this->handleEntity($orgEntities,$doc->name_organization_t,'Organization');
				}
			
				foreach ($orgEntities as $name=>$vals) { 
          $output .= '<li><b>'.$name.'</b>:&nbsp;'; 
          foreach ($vals as $val) { 
            $val = trim(preg_replace('/[,\s]+/',' ',$val));
            $output.= l($val,'islandora/solr/search/name_organization_t:"'.$val.'"').'&nbsp; ';
          }
          $output .= '</li>';				
				}
				
			}
 
      if (isset($doc->related_item_identifier_t))
      {
				$c_pid= $doc->related_item_identifier_t;
				if (is_array($doc->related_item_identifier_t))  {
					$c_pid = $doc->related_item_identifier_t[0];
				}
				$c_pid=htmlspecialchars(preg_replace('/\//',':',$c_pid),ENT_QUOTES,'utf-8');
				if (!preg_match('/^fedora\/(repository\/)?'.$c_pid.'/',$arg)) {
					$output.='<li><b>Collection:</b>&nbsp;<a href="'.$base_url.'/fedora/'.$c_pid.'">'.$doc->related_item_title_t.'</a></li>';
				}
      }      
            
      $output.='</ul></div></td></tr>';
      
     }
    $output.='</table></div>';
     
    return $output;
  }

	function handleEntity(&$entities, $e,$type) {
    $name=$type; $val = $e;
		if (preg_match('/^(.+)\,\s+([a-z]{3})$/',$e,$matches)) {
      $name = $this->lookupMarcrelator($matches[2]).' - '.$type;
			$val  =$matches[1];
    } 
		
		if (is_array($entities[$name])) { 
			$entities[$name][]=$val; 
		} else {
			$entities[$name]=array($val);
		}
	}


}

?>
