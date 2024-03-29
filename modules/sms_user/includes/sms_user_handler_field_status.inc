<?php

/**
* @file
* Views handler to display the sms number status.
*/

/**
 * Field handler to show the sms number status.
 *
 * @ingroup views_field_handlers
 */
class sms_user_handler_field_status extends views_handler_field {
  function render($values) {
    $status = $values->{$this->field_alias};
    $status_defined = array(
      SMS_USER_PENDING    => t('Pending'),
      SMS_USER_CONFIRMED  => t('Confirmed'),
      SMS_USER_ACTIVE  => t('Confirmed and Active'),
    );
    return $status_defined[$status];
  }
}

