<?php
if($_GET['file'] == 'settings'){
  if($_SERVER['REQUEST_METHOD'] === 'GET'){
    $fh = fopen(  './_settings.json' , "r" );
    $contents = fread( $fh, filesize(  './_settings.json'  ) );
    fclose( $fh );
    header('Content-type: application/json');
    echo $contents ;
  }else if($_SERVER['REQUEST_METHOD'] === 'PUT'){
    $fh = fopen( './_settings.json' , "w" );
    $contents = file_get_contents("php://input");
    $contents = json_decode( $contents ) ;
    $contents = json_encode( $contents , JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES ) ;
    fwrite( $fh , $contents );
    fclose( $fh );
  }
}
if($_GET['file'] == 'names'){
  if($_SERVER['REQUEST_METHOD'] === 'GET'){
    $fh = fopen(  './_names.json' , "r" );
    $contents = fread( $fh, filesize(  './_names.json'  ) );
    fclose( $fh );
    header('Content-type: application/json');
  //  echo $contents ;
    echo $contents;

  }else if($_SERVER['REQUEST_METHOD'] === 'PUT'){
    $fh = fopen( './_names.json' , "w" );
    $contents = file_get_contents("php://input");
    $contents = json_decode( $contents ) ;
    $contents = json_encode( $contents , JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES ) ;
    fwrite( $fh , $contents );
    fclose( $fh );
  }
}

?>
